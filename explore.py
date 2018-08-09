#!/usr/bin/python3

import os, subprocess, sys, random, string
from time import time

def buildsimulator(parameters="DEFINES=-DSize=1024 -DBlocksize=32 -DAssociativity=4 -DPolicy=2"):
	"return a string with newly build simulator or an empty string if build failed"
	try:
		subprocess.check_call(["make", parameters])
		executable = "temp_"+''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(10))
		subprocess.check_call(["cp", "comet.sim", executable])
		return executable
	except:
		print("Failed to build with parameters", parameters)
		return ""

def testbenchmark():
	progs = os.listdir("benchmarks/build")
	progs[:] = [p for p in progs if p.endswith(".riscv")]

	global res
	res = {p : [] for p in progs}

	for assoc in (1,2,4):
		for strpolicy in ("lru", "random", "fifo"):
			policytodefine = {"NONE" : 0, "FIFO" : 1, "LRU" : 2, "RANDOM" : 3}
			policy = policytodefine[strpolicy.upper()]
			for blocksize in (2,4,8,16):	# blocksize / 4
				for cachesize in (1024, 2048, 4096, 8192, 16384, 32768, 65536):
					makeparameters = "DEFINES=-DSize={cachesize} -DBlocksize={blocksize} -DAssociativity={assoc} -DPolicy={policy}".format(**locals())
					
					executable = buildsimulator(makeparameters)
					if executable == "":
						print("Error building with parameters :", makeparameters)
						continue
					
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
							
							res[p].append( (cachesize, strpolicy, assoc, blocksize, pcycles, pcycles/pinstructions) )
							
						except BaseException as e:
							print("Error for", p, ":", e)
							subprocess.check_call(["rm", executable])
							raise e

					subprocess.check_call(["rm", executable])

			if assoc == 1:
				break
		
	with open("res.txt", "a") as output:
		for p in progs:
			print(p, file=output)
			for el in res[p]:
				print(el, file=output)
	with open("testbenchmark.txt", "w") as output:
		for p in progs:
			print(p, file=output)
			for el in res[p]:
				print(el, file=output)

def mediabenchmark():
	benchmarks = ["adpcm", "epic", "g721", "jpeg", "mesa", "mpeg2", "rasta"]
	benchs = []
	
	for b in benchmarks:
		scripts = os.listdir("../Mediabench/"+b+"/exec")
		scripts = [b+"/"+s for s in scripts if s.endswith(".sh")]
		benchs.extend(scripts)
	
	global res
	res = {b : [] for b in benchs}

	for assoc in (1,2,4):
		for strpolicy in ("lru", "random", "fifo"):
			policytodefine = {"NONE" : 0, "FIFO" : 1, "LRU" : 2, "RANDOM" : 3}
			policy = policytodefine[strpolicy.upper()]
			for blocksize in (8,16,32,64):
				for cachesize in (1024, 2048, 4096, 8192, 16384, 32768, 65536):
					makeparameters = "DEFINES=-DSize={cachesize} -DBlocksize={blocksize} -DAssociativity={assoc} -DPolicy={policy}".format(**locals())
					executable = buildsimulator(makeparameters)
					if executable == "":
						print("Error building with parameters :", makeparameters)
						continue
					os.chdir("../Mediabench/"+benchmarks[0]+"/exec")
					for b in benchmarks:
						try:
							os.chdir("../../"+b+"/exec")
							scripts = os.listdir()
							scripts = [s for s in scripts if s.endswith(".sh")]
							for s in scripts:
								print("trying", b+"/exec/"+s, "in", os.getcwd())
								with subprocess.Popen(["/bin/bash", s, "../../../comet/"+executable], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True) as output:
									out = ""
									for line in output.stdout:
										out += line
								
								temps = out.split('\n')[-2]
								print(temps)
								assert temps.startswith("Successfully"), "No timing information found"
								
								pinstructions = int(temps.split()[2])
								pcycles = int(temps.split()[5])
								
								res[b+"/"+s].append( (cachesize, strpolicy, assoc, blocksize, pcycles, pcycles/pinstructions) )
							
						except BaseException as e:
							print("Error for", b, ":", e)
							os.chdir("../../../comet/")
							subprocess.check_call(["rm", executable])
							raise e
					
					os.chdir("../../../comet/")
					subprocess.check_call(["rm", executable])			
									
			if assoc == 1:
				break
		
	with open("res.txt", "a") as output:
		for p in progs:
			print(p, file=output)
			for el in res[p]:
				print(el, file=output)
	with open("mediabench.txt", "w") as output:
		for p in progs:
			print(p, file=output)
			for el in res[p]:
				print(el, file=output)
