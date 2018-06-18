#!/usr/bin/python3

import os, subprocess, sys

if __name__ == "__main__":
	for assoc in (1,2,4):
		for policy in ("lru", "random", "fifo"):
			for blocksize in (8,16,32,64):
				for cachesize in (1024, 2048, 4096, 8192, 16384, 32768, 65536):
					print(assoc, cachesize, blocksize, policy)
					checkparameters = "-m -c={cachesize} -b={blocksize} -a={assoc} -p={policy}".format(**globals())
					cmd = ["./check.py"] + checkparameters.split()
					subprocess.call(cmd)
		
			if assoc == 1:
				break
			
			
		
