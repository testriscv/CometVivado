#!/usr/bin/python3

import os, subprocess, sys, random, string
from multiprocessing import Process, Queue, Lock
from time import time, sleep

Associativity = (1,2,4)
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
	progs[:] = [p for p in progs if p.endswith(".riscv")]
	
	for p in progs:	
		try:
			print("trying", p)
			with subprocess.Popen(["./"+executable, "-f", "benchmarks/build/"+p], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True) as output:
				out = ""
				for line in output.stdout:
					out += line
			
			temps = out.split('\n')[-2]
			assert temps.startswith("Successfully"), "No timing information found"
			
			pinstructions = int(temps.split()[2])
			pcycles = int(temps.split()[5])
			
			q.put( (p, cachesize, strpolicy, assoc, blocksize, pcycles, pcycles/pinstructions) )
			
		except BaseException as e:
			print("Error for", p, ":", e)
			subprocess.check_call(["rm", executable])
			raise e

	subprocess.check_call(["rm", executable])

def testbenchmark():
	progs = os.listdir("benchmarks/build")
	progs[:] = [p for p in progs if p.endswith(".riscv")]
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
				
				temps = out.split('\n')[-2]
				print(temps)
				assert temps.startswith("Successfully"), "No timing information found"
				
				pinstructions = int(temps.split()[2])
				pcycles = int(temps.split()[5])
				
				q.put( (b+"/"+s, cachesize, strpolicy, assoc, blocksize, pcycles, pcycles/pinstructions) )
			
		except BaseException as e:
			print("Error for", b, ":", e)
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
		for b in benchs:
			for el in res[b]:
				print(b, *el, file=output)
	with open("mediabench.txt", "w") as output:
		for b in benchs:
			for el in res[b]:
				print(b, *el, file=output)

