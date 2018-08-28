#!/usr/bin/python3

import os, subprocess, sys, random, string, resource, re
from multiprocessing import Process, Queue, Lock
from time import time, sleep

def timetostr(time):
	if time > 3600:
		return "{}h{:02d}m{:02d}s".format(int(time/3600), int((time%3600)/60), int(time%60))
	elif time > 60:
		return "{}m{:02d}s".format(int(time/60), int(time%60))
	elif time > 1:
		return "{}s{:03d}ms".format(int(time), int((time-int(time))*1000))
	else:
		return "{}ms".format(int(time*1000))

Associativity = (2,4)
BlockSize = (2,4,8,16)	# 8,16,32,64
StrPolicy = ("fifo", "lru", "random")
CacheSize = (1024,2048,4096,8192,16384,32768,65536)

def buildsimulator(cachesize=1024, blocksize=8, assoc=4, strpolicy="lru"):
	"return a string with newly build simulator or an empty string if build failed"
	try:
		policytodefine = {"NONE" : 0, "FIFO" : 1, "LRU" : 2, "RANDOM" : 3}
		policy = policytodefine[strpolicy.upper()]
		makeparameters = "DEFINES=-DSize={cachesize} -DBlocksize={blocksize} -DAssociativity={assoc} -DPolicy={policy}".format(**locals())
		subprocess.check_call(["make", makeparameters])
		# ~ executable = "temp_"+''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(10))
		executable = "temp_"+str(cachesize)+strpolicy+str(assoc)+"x"+str(4*blocksize)
		subprocess.check_call(["cp", "comet.sim", executable])
		return executable
	except:
		print("Failed to build with parameters", cachesize, strpolicy, assoc, blocksize)
		return ""
		
def runtestbenchmark(executable, q, cachesize, strpolicy, assoc, blocksize):
	progs = os.listdir("benchmarks/build")
	progs[:] = [p for p in progs if p.endswith(".riscv32")]
	
	for p in progs:	
		try:
			print("trying", p)
			with subprocess.Popen(["./"+executable, "-f", "benchmarks/build/"+p], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True) as output:
				out = ""
				for line in output.stdout:
					out += line
			
			out = out.split('\n')
			for i, line in enumerate(reversed(out)):
				if line.startswith("Successfully executed "):
					temps = line
					break
				elif line.startswith("Cache Statistics :"):
					cachestart = len(out)-i
			else:
				assert False, "No timing information found"
			print(temps)
			
			d = ("Miss", "Hit", "Cachemem Read", "Cachemem Write", "Ctrlmem Read", "Ctrlmem Write", "Mainmem Read", "Mainmem Write")
			icache = {k : 0 for k in d}
			dcache = {k : 0 for k in d}
			for i in range(cachestart+1, len(out)):
				line = out[i]
				l = [m for m in re.split(r"[:\s]", line) if m]
				k = ""
				for j,el in enumerate(l):
					try:
						datum = int(el)
						break
					except:
						k += el+" "
					
				k = k.strip()
				if k in d:
					icache[k] = datum
					dcache[k] = int(l[j+1])
			
			pinstructions = int(temps.split()[2])
			pcycles = int(temps.split()[5])
			
			q.put( (p, cachesize, strpolicy, assoc, 4*blocksize, pcycles, pcycles/pinstructions, icache, dcache) )
			
		except BaseException as e:
			print("\033[1;31m Error {} : {} \033[0m for {}".format(type(e), e, p))
			subprocess.check_call(["rm", executable])
			raise e

	subprocess.check_call(["rm", executable])

def testbenchmark():
	progs = os.listdir("benchmarks/build")
	progs[:] = [p for p in progs if p.endswith(".riscv32")]
	global res
	res = {p : [] for p in progs}
	
	processes = []
	q = Queue()
	
	for assoc in Associativity:
		for strpolicy in StrPolicy:
			for blocksize in BlockSize:
				for cachesize in CacheSize:
					executable = buildsimulator(cachesize, blocksize, assoc, strpolicy)
					if executable == "":
						print("Error building with parameters :", cachesize, strpolicy, assoc, blocksize)
						continue
					
					processes.append(Process(target=runtestbenchmark, args=(executable,q,cachesize,strpolicy,assoc,blocksize) ))
					processes[-1].start()
					try:
						while True:
							foo = q.get_nowait()
							res[foo[0]].append(foo[1:])
							print(q.qsize())
					except:
						pass
					while sum(1 for p in processes if p.is_alive()) == 4:
						# ~ print("Limiting to 4 processes")
						try:
							while True:
								foo = q.get_nowait()
								res[foo[0]].append(foo[1:])
								print(q.qsize())
						except:
							pass
						sleep(1)

			if assoc == 1:
				break

	while any(p.is_alive() for p in processes):
		try:
			while True:
				foo = q.get_nowait()
				res[foo[0]].append(foo[1:])
				print(q.qsize())
		except:
			pass
		sleep(1)
		
	try:
		while True:
			foo = q.get_nowait()
			res[foo[0]].append(foo[1:])
	except:
		pass
		
	any(p.join() for p in processes)
	
	try:
		while True:
			foo = q.get_nowait()
			res[foo[0]].append(foo[1:])
	except:
		pass
	
	with open("res.txt", "a") as output:
		for p in progs:
			for el in res[p]:
				print(p, *el, file=output)
	with open("testbenchmark.txt", "w") as output:
		for p in progs:
			for el in res[p]:
				print(p, *el, file=output)
				
def runmediabenchmark(executable, q, cachesize, strpolicy, assoc, blocksize):
	benchmarks = ["adpcm", "epic", "g721", "jpeg", "mesa", "mpeg2", "rasta"]
		
	os.chdir("../Mediabench/"+benchmarks[0]+"/exec")
	for b in benchmarks:
		try:
			os.chdir("../../"+b+"/exec")
			scripts = os.listdir()
			scripts = [s for s in scripts if s.endswith(".sh")]
			for s in scripts:
				print("trying", b+"/exec/"+s, "in", os.getcwd())
				with subprocess.Popen(["./"+s, "../../../comet/"+executable], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True) as output:
					out = ""
					for line in output.stdout:
						out += line
				
				out = out.split('\n')
				for i, line in enumerate(reversed(out)):
					if line.startswith("Successfully executed "):
						temps = line
						break
					elif line.startswith("Cache Statistics :"):
						cachestart = len(out)-i
				else:
					assert False, "No timing information found"
				print(temps)
				
				d = ("Miss", "Hit", "Cachemem Read", "Cachemem Write", "Ctrlmem Read", "Ctrlmem Write", "Mainmem Read", "Mainmem Write")
				icache = {k : 0 for k in d}
				dcache = {k : 0 for k in d}
				for i in range(cachestart+1, len(out)):
					line = out[i]
					l = [m for m in re.split(r"[:\s]", line) if m]
					k = ""
					for j,el in enumerate(l):
						try:
							datum = int(el)
							break
						except:
							k += el+" "
						
					k = k.strip()
					if k in d:
						icache[k] = datum
						dcache[k] = int(l[j+1])
				
				pinstructions = int(temps.split()[2])
				pcycles = int(temps.split()[5])
				
				q.put( (b+"/"+s, cachesize, strpolicy, assoc, 4*blocksize, pcycles, pcycles/pinstructions, icache, dcache) )
			
		except BaseException as e:
			print("\033[1;31m Error {} : {} \033[0m for {}".format(type(e), e, b))
			os.chdir("../../../comet/")
			subprocess.check_call(["rm", executable])
			raise e
	
	os.chdir("../../../comet/")
	subprocess.check_call(["rm", executable])

def mediabenchmark():
	benchmarks = ["adpcm", "epic", "g721", "jpeg", "mesa", "mpeg2", "rasta"]
	benchs = []
	
	for b in benchmarks:
		scripts = os.listdir("../Mediabench/"+b+"/exec")
		scripts = [b+"/"+s for s in scripts if s.endswith(".sh")]
		benchs.extend(scripts)
	
	global res
	res = {b : [] for b in benchs}
	
	processes = []
	q = Queue()

	for assoc in Associativity:
		for strpolicy in StrPolicy:
			for blocksize in BlockSize:
				for cachesize in CacheSize:
					executable = buildsimulator(cachesize, blocksize, assoc, strpolicy)
					if executable == "":
						print("Error building with parameters :", makeparameters)
						continue
					
					processes.append(Process(target=runmediabenchmark, args=(executable,q,cachesize,strpolicy,assoc,blocksize) ))
					processes[-1].start()
					try:
						while True:
							foo = q.get_nowait()
							res[foo[0]].append(foo[1:])
					except:
						pass
					while sum(1 for p in processes if p.is_alive()) == 4:
						# ~ print("Limiting to 4 processes")
						try:
							while True:
								foo = q.get_nowait()
								res[foo[0]].append(foo[1:])
						except:
							pass
						sleep(1)
					break			
			if assoc == 1:
				break
				
	while any(p.is_alive() for p in processes):
		try:
			while True:
				foo = q.get_nowait()
				res[foo[0]].append(foo[1:])
		except:
			pass
		sleep(1)
		
	try:
		while True:
			foo = q.get_nowait()
			res[foo[0]].append(foo[1:])
	except:
		pass
		
	any(p.join() for p in processes)
	
	try:
		while True:
			foo = q.get_nowait()
			res[foo[0]].append(foo[1:])
	except:
		pass
		
	with open("res.txt", "a") as output:
		for b in benchs:
			for el in res[b]:
				print(b, *el, file=output)
	with open("mediabench.txt", "w") as output:
		for b in benchs:
			for el in res[b]:
				print(b, *el, file=output)

def spikemedia():
	benchmarks = ["adpcm", "epic", "g721", "jpeg", "mesa", "mpeg2", "rasta"]
	benchs = []
	
	for b in benchmarks:
		scripts = os.listdir("../Mediabench/"+b+"/exec")
		scripts = [b+"/"+s for s in scripts if s.endswith(".sh")]
		benchs.extend(scripts)
	
	global temps
	temps = {b : (0,0) for b in benchs}
	
	os.chdir("../Mediabench/"+benchmarks[0]+"/exec")
	for b in benchmarks:
		try:
			os.chdir("../../"+b+"/exec")
			scripts = os.listdir()
			scripts = [s for s in scripts if s.endswith(".sh")]
			for s in scripts:
				print("trying", b+"/exec/"+s, "in", os.getcwd())
				start = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
				with subprocess.Popen(["./"+s], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) as foo:
					pass					
				end = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
				t = end-start
				print("Done Comet", b+"/"+s, "     in", timetostr(end-start))
				
				start = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
				with subprocess.Popen(["./"+s[:-2]+"spikesh"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) as foo:
					pass					
				end = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime
				print("Done Spike", b+"/"+s[:-2]+"spikesh", "in", timetostr(end-start))
				
				temps[b+"/"+s] = (t, end-start)
				
			
		except BaseException as e:
			print("Error for", b, ":", e)
			os.chdir("../../../comet/")
			raise e
	
	os.chdir("../../../comet/")
	
	with open("times_med_spike.txt", "w") as t:
		print("Benchmarks Comet Spike", file=t)
		for b in temps:
			print(b, *temps[b], file=t)
