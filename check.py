#!/usr/bin/python3

import os, subprocess, sys, argparse
import random, string, struct

class Boundaries:
	"""Class to represents discontinuous set.
	Example: [0;12] U [125;256]"""
	def __init__(self, m=None, M=None):
		if m is not None and M is not None:
			self.bounds = [(m, M)]
		else:
			self.bounds = []

	def __repr__(self):
		if len(self.bounds):
			return " U ".join("[{:06x};{:06x}]".format(m, M) for m, M in self.bounds)
		else:
			return "Empty"
		
	def __contains__(self, item):
		for m, M in self.bounds:
			if item >= m and item <= M:
				return True
		return False
	
	def __iter__(self):
		for m, M in self.bounds:
			yield m
			yield M
	
	def append(self, m, M):
		self.bounds.append( (m, M) )
		
##########################
## Replacement policies ##
##########################

def noinsert(pol, sett, way):			# called on insertion if policy has no insertion
	pass

def nopromotion(pol, sett, way):		# called on promotion if policy has no promotion
	pass

def lruinsert(pol, sett, way):			# called only on insertion
	lrupromote(pol, sett, way)

def lrupromote(pol, sett, way):			# called only on promotion
	ind = pol[sett].index(way)
	for i in range(ind,0,-1):
		pol[sett][i] = pol[sett][i-1]
	pol[sett][0] = way
	
def lruselect(pol, sett, way):
	return pol[sett][-1]
	
def randomselect(pol, sett, way):		# called only on selection
	replaced = pol[0] & (assoc - 1)
	pol[0] = ((bool(pol[0] & 0x80000000) ^ bool(pol[0] & 0x200000) ^ \
	bool(pol[0] & 2) ^ bool(pol[0] & 1)) | (pol[0] << 1)) & 0xFFFFFFFF
	
	return replaced
		
def fifoselect(pol, sett, way):			# called only on selection
	replaced = pol[sett]
	pol[sett] = (pol[sett] + 1) % assoc
	
	return replaced

##########################
##   Script functions   ##
##########################

def init(_assoc = 4, _cachesize = 1024, _blocksize = 32, _policy = "lru"):
	global assoc, sets, policypromote, policyselect, policyinsert, policy, cachesize, blocksize
	assoc = _assoc if _assoc is not None else 4
	cachesize = _cachesize if _cachesize is not None else 1024
	blocksize = _blocksize//4 if _blocksize is not None else 8
	sets = int((cachesize/(blocksize*4))/assoc)
	policy = _policy if _policy is not None else "lru"
	policy = policy.lower()
	
	try:
		policypromote = getattr(sys.modules["__main__"], policy+"promote")
	except:
		try:
			policypromote = getattr(sys.modules[__name__], policy+"promote")
		except:
			policypromote = nopromotion
			
	try:
		policyinsert = getattr(sys.modules["__main__"], policy+"insert")
	except:
		try:
			policyinsert = getattr(sys.modules[__name__], policy+"insert")
		except:
			policyinsert = noinsert
	
	try:
		policyselect = getattr(sys.modules["__main__"], policy+"select")
	except:
		policyselect = getattr(sys.modules[__name__], policy+"select")

	pmap = {"none":0, "fifo":1, "lru":2, "random":3}
	policy = pmap.get(policy, 0)
	
	return assoc, cachesize, blocksize, policy
	
def formatread(dmem, ad, size, sign):
	read = 0
	for i, j in enumerate(range(ad, ad+size+1)):
		read |= (dmem[j][0] << (8*i))
		# ~ print(i, read)
	
	# ~ print(sign, size, read & (1 << (8*(i+1)-1)))
	if sign and read & (1 << (8*(i+1)-1)):
		if size == 0:
			read |= 0xFFFFFF00
		elif size == 1:
			read |= 0xFFFF0000
		# ~ print(read)

	return read
	
def formatwrite(dmem, ad, size, val):
	for i, j in enumerate(range(ad, ad+size+1)):
		foo = (val & (0xFF << (8*i))) >> (8*i)
		dmem[j] = (foo, True)
		
	return formatread(dmem, ad & 0xFFFFFFFC, 3, False)
	
def readpreambule(fichier, prog):
	global lues
	lues = list()
	try:
		fichier.seek(0)			# go to start of file
		pipe = False
	except:
		fichier = fichier.stdout
		pipe = True
		print("Got pipelined file")
	i = 2
	line = fichier.readline()
	lues.append(line)
	while not line.startswith("filling"):
		line = fichier.readline()
		lues.append(line)
	# ~ assert lues[0].split('/')[-1][:-1] == prog, "Wrong matching with "\
	# ~ "program's name : {}".format(prog)
	ibound = Boundaries()
	dbound = Boundaries()
	while line.startswith("filling"):
		l = line.split()
		if l[1] == "instruction":
			ibound.append(int(l[-3], 16), int(l[-1], 16))
		elif l[1] == "data":
			dbound.append(int(l[-3], 16), int(l[-1], 16))
		else:
			assert False, "{} line {} : {} Neither data or instruction found in preamble"\
			.format(prog, i, line)
		line = fichier.readline()
		lues.append(line)
		i += 1
	
	assert line == "instruction memory :\n", "{} line {} : {} Expected instruction memory"\
			.format(prog, i, line)
	
	line = fichier.readline()
	lues.append(line)
	i += 1
	N = 2**20
	imem = [0] * N
	while not line.startswith("data"):
		l = line.split()
		ad = int(l[0], 16)
		val = int(l[2], 16)
		imem[ad] = val
		line = fichier.readline()
		lues.append(line)
		i += 1
	
	assert line == "data memory :\n", "{} line {} : {} Expected data memory"\
			.format(prog, i, line)
			
	line = fichier.readline()
	lues.append(line)
	i += 1
	dmem = [(0, False)] * N
	while not line.startswith("end"):
		l = line.split()
		ad = int(l[0], 16)
		val = int(l[2], 16)
		dmem[ad] = (val, False)
		line = fichier.readline()
		lues.append(line)
		i += 1
	
	return imem, dmem, ibound, dbound, i, pipe

def parsefile(fichier, prog, cache=True):
	global ipolicy, dpolicy, imem, dmem, ibound, dbound, i
	imem, dmem, ibound, dbound, i, pipe = readpreambule(fichier, prog)
	
	if pipe:
		fichier = fichier.stdout
	
	reg = [0] * 32
	reg[2] = 0x000F0000
	global ipath, dpath, rpath
	rpath = [[0]*32]
	ipath = list()	
	ipath.append( (0,0,0) )			# pc, registre modifié, nouvelle valeur
	dpath = list()# (0,0,False,0,False)	# address, valeur, (0:read, 1:write), datasize, sign extension
		
	#dmem = {}	
	#imem = {}
	endmem = {}
	
	if cache:
		if policy == 0:			# No policy
			pass
		elif policy == 1:		# FIFO
			dpolicy = [0 for i in range(sets)]
			ipolicy = [0 for i in range(sets)]
		elif policy == 2:		# LRU
			dpolicy = [[i for i in range(assoc)] for i in range(sets)]
			ipolicy = [[i for i in range(assoc)] for i in range(sets)]
		elif policy == 3:		# RANDOM
			dpolicy = [0xF2D4B698]		# made as a list to be modified by function
			ipolicy = [0xF2D4B698]


	try:
		for line in fichier:
			lues.append(line)
			i += 1
			if line.startswith("W"):		# WriteBack
				l = line.split()
				if len(l) > 1:
					ad = int(l[1][1:], 16)
					ins = int(l[2], 16)
					numins = int(l[3][1:-1])
					assert ad in ibound, "{} line {} : {} Instruction "\
					"out of bound @{:06x}".format(prog, i, line, ad)
					
					assert ins == imem[ad], "{} line {} : {} Instruction is incorrect, "\
					"expected {:08x} got {:08x} @{:06x}".format(prog, i, line, imem[ad], ins, ad)
						
					if ipath[-1][0] != ad:
						line = fichier.readline()
						lues.append(line)
						i += 1
						l = line.split()
						changed = False
						r = [0]*32
						for tmp in l[1:]:
							foo = tmp.split(':')
							idx = int(foo[0])
							val = int(foo[1], 16)
							
							r[idx] = val
							
							if reg[idx] != val:
								assert changed == False, "{} line {}: {} Multiple registers "\
								"changed @{:06x}".format(prog, i, line, ad)
								ipath.append((ad, idx, val))
								reg[idx] = val
								changed = True
								
						rpath.append(r)
						if not changed:
							ipath.append((ad, 0, 0))
							
			elif line.startswith("i"):
				l = line.split()
				ad = int(l[1][1:], 16)
				ins = int(l[2], 16)
				if ad not in ibound:
					print("{} line {} : {} Instruction "\
				"out of bound @{:06x}".format(prog, i, line, ad))
				
				assert ins == imem[ad], "{} line {} : {} Instruction is incorrect, "\
				"expected {:08x} got {:08x} @{:06x}".format(prog, i, line, imem[ad], ins, ad)
				
				if cache:
					sett = int(l[-2])
					way = int(l[-1])
					
					policypromote(ipolicy, sett, way)		# promotion of the accessed way
			
			elif line.startswith("d"):		# data access
				#dR{Size}	@{ad}	{memory}	{formattedread}	{signextension}
				#dW{Size}	@{ad}	{memory}	{writevalue}	{formattedwrite}
				l = line.split()
				we = l[0][1] == "W"
				size = int(l[0][2])
				ad = int(l[1][1:], 16)
				mem = int(l[2], 16)
				
				if we:
					writevalue = int(l[3], 16)
					fwrite = int(l[4], 16)
					sign = False
				else:
					fread = int(l[3], 16)
					sign = l[4] == "true"
					
				if cache:
					sett = int(l[-2])
					way = int(l[-1])
					
					policypromote(dpolicy, sett, way)		# promotion of the accessed way
					
				# ~ print(line)
				# ~ print("W" if we else "R", size, hex(ad), hex(mem), sign)
					
				if ad & 1:
					assert size == 0, "{} line {} : {} address misalignment @{:06x}"\
					.format(prog, i, line, ad)
				elif ad & 2:
					assert size <= 1, "{} line {} : {} address misalignment @{:06x}"\
					.format(prog, i, line, ad)
				
					
				if we:
					check = formatwrite(dmem, ad, size, writevalue)
					assert check == fwrite, "{} line {} : {} Formatted write failed @{:06x},"\
					" expected {:02x} got {:02x}".format(prog, i, line, ad, check, fwrite)
					
					for n, a in enumerate(range(ad, ad+size+1)):
						val = (writevalue & (0xFF << (8*n))) >> (8*n)
						dpath.append( (a, val, we, size, sign) )
				else:
					check = formatread(dmem, ad, size, sign)
					assert check == fread, "{} line {} : {} Formatted read failed @{:06x},"\
					" expected {:02x} got {:02x}".format(prog, i, line, ad, check, fread)
					
					readuninit = False
					for n, a in enumerate(range(ad, ad+size+1)):
						# ~ print(n, hex(a), hex(dmem[a][0]), hex((fread >> (8*n)) & 0xFF))
						val = (fread >> (8*n)) & 0xFF
						assert dmem[a][0] == val, "{} line {} : {} Read value is incorrect"\
						" @{:06x}, expected {:02x} got {:02x}".format(prog, i, line, a, dmem[a][0], val)
					
						if not dmem[a][1]:
							if not a in dbound:
								if not readuninit:
									print("{} line {} : {} Reading unitialized memory @{:06x}"\
									.format(prog, i, line, ad))
									readuninit = True
								else:
									print(" Reading unitialized memory @{:06x}".format(a))
									
						dpath.append( (a, val, we, size, sign) )
			
			elif line.startswith("c"):			# Cache
				l = line.split()
				ad = int(l[1][1:], 16)
				sett = int(l[9])
				way = int(l[10])
				
				if l[0][1] == "d":				# data cache only
					assert way == policyselect(dpolicy, sett, way), "{} line {} : {} Replacing wrong way @{:06x},"\
					" expected {} got {} in set {}".format(prog, i, line, ad, policyselect(dpolicy, sett, way), way, sett)
					if "fetching" in l:			# prevent double "hit" when writebacking then fetching to the same way
						policyinsert(dpolicy, sett, way)
				elif l[0][1] == "i":
					assert way == policyselect(ipolicy, sett, way), "{} line {} : {} Replacing wrong way @{:06x},"\
					" expected {} got {} in set {}".format(prog, i, line, ad, policyselect(ipolicy, sett, way), way, sett)
					if "fetching" in l:
						policyinsert(ipolicy, sett, way)
				else:
					assert False
					
				
				
				
			# ~ elif line.startswith("M"):		# Memorization
				# ~ pass
		
			# ~ elif line.startswith("E"):		# Execute
				# ~ pass
				
			# ~ elif line.startswith("D"):		# Decode
				# ~ pass
				
			# ~ elif line.startswith("F"):		# Fetch
				# ~ pass
				
			elif line.startswith("Su"):
				l = line.split()
				cycles = " ".join(l[-2:])
				cycles = int(cycles.split()[0])
				break
	except BaseException as e:
		print(str(type(e)).split("'")[1], "on line", i,":", e)
		raise e
	
	cpi = cycles/numins
	print("{} done {} instructions in {} cycles ({:.2f} CPI)".format(prog, numins, cycles, cpi))
	
	line = fichier.readline()
	lues.append(line)
	i += 1
	assert line == "memory : \n"
	for line in fichier:					# read end memory
		lues.append(line)
		i += 1
		l = line.split()
		ad = int(l[0], 16)
		val = int(l[2], 16)
		endmem[ad] = val
		assert dmem[ad][0] == endmem[ad], "{}\n Memory not consistent @{:06x} " \
		": expected {:02x} got {:02x}".format(prog, ad, dmem[ad][0], endmem[ad])
	
	# rebuilds dmem from dpath
	bis = {}
	for data in dpath:						# address, valeur, (0:read, 1:write), datasize, sign extension
		if data[2]:
			bis[data[0]] = (data[1], True)
		elif data[0] in bis:
			assert bis[data[0]][0] == data[1], "{}\n Read value is incorrect, "\
			"expected {:02x} got {:02x} @{:06x}".format(prog, data[1], bis[data[0]][0], data[0])
		else:
			bis[data[0]] = (data[1], False)
		
	for ad in bis:
		assert dmem[ad] == bis[ad], "{}\n Memory not consistent @{:06x} " \
		": expected {} got {}".format(prog, ad, dmem[ad], bis[ad])		
		
	return ipath, dpath, rpath, imem, dmem, endmem, cycles, cpi

def checkoutput(name, cache):
	with open("output.log", "r") as f:
		a = parsefile(f, name, cache)
		return a
		
def parseall(cached):
	progs = os.listdir("benchmarks/build")
	progs[:] = [p for p in progs if p.endswith(".riscv")]

	a = dict()
	for p in progs:
		if p.split('_')[0] not in a:
			a[p.split('_')[0]] = [p]
		else:
			a[p.split('_')[0]].append(p)
	foo = list()
	for l in a:
		a[l].sort(key=lambda p: int(p.split('_')[2].split('.')[0]))
		foo.append(a[l])
	
	progs = [val for tup in zip(*foo) for val in tup]
	del a, foo

	
	executable = "temp_"+''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(10))
	subprocess.check_call(["cp", "comet.sim", executable])
	
	cycles = list()
	cpis = list()
	for p in progs:	
		try:
			with subprocess.Popen(["./"+executable, "benchmarks/build/"+p], stdout=subprocess.PIPE, universal_newlines=True) as output:
				global dmem
				ipath, dpath, rpath, imem, dmem, endmem, cycle, cpi = parsefile(output, p, cached)
				
			cycles.append(cycle)
			cpis.append(cpi)
			
			with open("benchmarks/res/"+p[:-6], "r") as resfile:
				text = resfile.read().split()
				global res
				if "char" in p:
					res = [int(i) & 0xFF for i in text]
				elif "short" in p:
					text = [int(i) & 0xFFFF for i in text]
					res = []
					for i in text:
						assert i >= 0
						res.append(i & 0x00FF)
						res.append((i & 0xFF00) >> 8)
				elif "int64" in p:
					text = [int(i) for i in text]
					res = []
					for i in text:
						assert i >= 0
						res.append(i & 0x00000000000000FF)
						res.append((i & 0x000000000000FF00) >> 8)
						res.append((i & 0x0000000000FF0000) >> 16)
						res.append((i & 0x00000000FF000000) >> 24)
						res.append((i & 0x000000FF00000000) >> 32)
						res.append((i & 0x0000FF0000000000) >> 40)
						res.append((i & 0x00FF000000000000) >> 48)
						res.append((i & 0xFF00000000000000) >> 56)
				elif "int" in p:
					text = [int(i) for i in text]
					res = []
					for i in text:
						assert i >= 0
						res.append(i & 0x000000FF)
						res.append((i & 0x0000FF00) >> 8)
						res.append((i & 0x00FF0000) >> 16)
						res.append((i & 0xFF000000) >> 24)
				elif "float" in p:
					text = [float(i) for i in text]
					res = []
					for i in text:
						i = struct.unpack('!i',struct.pack('!f',i))[0]
						res.append(i & 0x000000FF)
						res.append((i & 0x0000FF00) >> 8)
						res.append((i & 0x00FF0000) >> 16)
						res.append((i & 0xFF000000) >> 24)
						
						
			dmem = [el[0] for el in dmem]
			
			def sublist(sub, biglist):
				j = 0
				for i in range(len(biglist)):
					if biglist[i] == sub[j]:
						j += 1
						if j == len(sub):
							return True
					else:
						j = 0
				return False
				
			# ~ assert sublist(res, dmem), "{} result is incorrect".format(p)
			assert str(res)[1:-1] in str(dmem), "{} result is incorrect".format(p)
			
		except BaseException as e:
			subprocess.check_call(["rm", executable])
			with open("traces/{}cache_{}.log".format("no" if not cached else "", p), "w") as dump:
				print("Dumping to", dump.name)
				dump.write("".join(line for line in lues))
			raise e
	try:
		if not cached:
			with open("timesnocache.txt", "w") as times:
				print("{:<30s} {:>8s} {:>9s}".format("Benchmark", "Cycles", "CPI"), file = times)
				for i, p in enumerate(progs):
					print("{:<30s} {:>8d} {:>9.2f}".format(p, cycles[i], cpis[i]), file = times)
		else:
			timings = dict()
			with open("timesnocache.txt", "r") as times:
				times.readline()
				for line in times:
					l = line.split()
					if len(l) == 3:
						timings[l[0]] = (int(l[1]), float(l[2]))
			
			with open("res.txt", "a") as res:		
				print("\n==========================================================================\n",\
				 lues[0], lues[1], file = res)
				print("{:<30s} {:^21s} {:^21s}".format("Timings", "Uncached", "Cache"), file = res)
				print("{:<30s} {:>7s} {:>10s} {:>10s} {:>10s} {:>10s}".format("Benchmark", "Cycles",\
				"CPI", "Cycles", "CPI", "Speedup"), file = res)
		
				for i, p in enumerate(progs):
					print("{:<30s} {:7d} {:>10.2f} {:10d} {:>10.2f} {:>+10.0%}".format(p, timings[p][0],\
					timings[p][1], cycles[i], cpis[i], timings[p][1]/cpis[i]), file = res)
	except BaseException as e:
		subprocess.check_call(["rm", executable])
		print(lues[0], lues[1][:-1], "done")
		raise e

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument("-f", "--file", help="Parse only the given file")
	parser.add_argument("-m", "--make", help="Build before comparing", action="store_true")
	parser.add_argument("-n", "--nocache", help="Performs nocache comparison and build nocache if -m is set", action="store_true")
	parser.add_argument("-s", "--shell", help="Launch catapult in shell", action="store_true")
	parser.add_argument("-c", "--cache-size", help="Cache size in bytes", type=int)
	parser.add_argument("-a", "--associativity", help="Cache associativity", type=int)
	parser.add_argument("-b", "--blocksize", help="Cache blocksize in bytes", type=int)
	parser.add_argument("-p", "--policy", help="Replacement policy")

	args = parser.parse_args()

	init(args.associativity, args.cache_size, args.blocksize, args.policy)
	
	if args.file is not None:
		with open(args.file, "r") as file:
			ipath, dpath, rpath, imem, dmem, endmem, cycle, cpi = parsefile(file, args.file, not args.nocache)
		sys.exit(0)
	
	if args.make:
		makeparameters = "DEFINES=-DSize={cachesize} -DBlocksize={blocksize} -DAssociativity={assoc} -DPolicy={policy}".format(**globals())
		if args.nocache:
			makeparameters += " -Dnocache"
		subprocess.check_call(["make", makeparameters])
	
	parseall(not args.nocache)
