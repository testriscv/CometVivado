#!/usr/bin/python3

import argparse, os, sys
from math import log2
import subprocess
from multiprocessing import Process, Queue
from time import time, sleep

genMem = """flow package require MemGen

flow run /MemGen/MemoryGenerator_BuildLib {{
VENDOR           STMicroelectronics
RTLTOOL          DesignCompiler
TECHNOLOGY       {{28nm FDSOI}}
LIBRARY          ST_singleport_{memsize}x{bitwidth}
MODULE           ST_SPHD_BB_8192x32m16_aTdol_wrapper
OUTPUT_DIR       ../memories/
FILES {{
  {{ FILENAME /opt/DesignKit/cmos28fdsoi_29/memcut_28nm/C28SOI_SPHD_BB_170612/4.3-00.00/behaviour/verilog/SPHD_BB_170612.v         FILETYPE Verilog MODELTYPE generic PARSE 1 PATHTYPE abs STATICFILE 1 VHDL_LIB_MAPS work }}
  {{ FILENAME /opt/DesignKit/cmos28fdsoi_29/memcut_28nm/C28SOI_SPHD_BB_170612/4.3-00.00/behaviour/verilog/SPHD_BB_170612_wrapper.v FILETYPE Verilog MODELTYPE generic PARSE 1 PATHTYPE abs STATICFILE 1 VHDL_LIB_MAPS work }}
}}
VHDLARRAYPATH    {{}}
WRITEDELAY       0.194
INITDELAY        1
READDELAY        0.940
VERILOGARRAYPATH {{}}
INPUTDELAY       0.146
WIDTH            {bitwidth}
AREA             {area}
WRITELATENCY     1
RDWRRESOLUTION   UNKNOWN
READLATENCY      1
DEPTH            {memsize}
PARAMETERS {{
  {{ PARAMETER Words                TYPE hdl IGNORE 1 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER Bits                 TYPE hdl IGNORE 1 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER mux                  TYPE hdl IGNORE 1 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER Bits_Func            TYPE hdl IGNORE 0 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER mask_bits            TYPE hdl IGNORE 1 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER repair_address_width TYPE hdl IGNORE 1 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER Addr                 TYPE hdl IGNORE 0 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER read_margin_size     TYPE hdl IGNORE 1 MIN {{}} MAX {{}} DEFAULT 0 }}
  {{ PARAMETER write_margin_size    TYPE hdl IGNORE 1 MIN {{}} MAX {{}} DEFAULT 0 }}
}}
PORTS {{
  {{ NAME port_0 MODE ReadWrite }}
}}
PINMAPS {{
  {{ PHYPIN A     LOGPIN ADDRESS      DIRECTION in  WIDTH Addr      PHASE {{}} DEFAULT {{}} PORTS port_0 }}
  {{ PHYPIN CK    LOGPIN CLOCK        DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {{}} PORTS port_0 }}
  {{ PHYPIN CSN   LOGPIN CHIP_SELECT  DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {{}} PORTS {{}}     }}
  {{ PHYPIN D     LOGPIN DATA_IN      DIRECTION in  WIDTH Bits_Func PHASE {{}} DEFAULT {{}} PORTS port_0 }}
  {{ PHYPIN INITN LOGPIN UNCONNECTED  DIRECTION in  WIDTH 1.0       PHASE {{}} DEFAULT 1  PORTS {{}}     }}
  {{ PHYPIN Q     LOGPIN DATA_OUT     DIRECTION out WIDTH Bits_Func PHASE {{}} DEFAULT {{}} PORTS port_0 }}
  {{ PHYPIN WEN   LOGPIN WRITE_ENABLE DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {{}} PORTS port_0 }}
}}

}}"""

header = """set start [clock seconds]
logfile close
logfile open {name}.log
project set -incr_directory {name}
solution new -state initial
solution options set /Input/CompilerFlags {{-D __CATAPULT__=1 {nocache} -DSize={cachesize} -DAssociativity={associativity} -DBlocksize={blocksize} -DPolicy={policy}}}
solution options set /Input/SearchPath ../include
solution options set ComponentLibs/SearchPath ../memories -append
flow package require /SCVerify
solution file add ../src/core.cpp -type C++
solution file add ../src/reformeddm_sim.cpp -type C++ -exclude True
solution file add ../src/cache.cpp -type C++
solution file add ../src/simulator.cpp -type C++ -exclude True
solution file add ../src/elfFile.cpp -type C++ -exclude True
solution file add ../src/portability.cpp -type C++
go new
directive set -DESIGN_GOAL area
directive set -OLD_SCHED false
directive set -SPECULATE true
directive set -MERGEABLE true
directive set -REGISTER_THRESHOLD 4096
directive set -MEM_MAP_THRESHOLD 64
directive set -FSM_ENCODING none
directive set -REG_MAX_FANOUT 0
directive set -NO_X_ASSIGNMENTS true
directive set -SAFE_FSM false
directive set -ASSIGN_OVERHEAD 0
directive set -UNROLL no
directive set -IO_MODE super
directive set -REGISTER_IDLE_SIGNAL false
directive set -IDLE_SIGNAL {{}}
directive set -STALL_FLAG false
directive set -TRANSACTION_DONE_SIGNAL true
directive set -DONE_FLAG {{}}
directive set -READY_FLAG {{}}
directive set -START_FLAG {{}}
directive set -BLOCK_SYNC none
directive set -TRANSACTION_SYNC ready
directive set -DATA_SYNC none
directive set -RESET_CLEARS_ALL_REGS true
directive set -CLOCK_OVERHEAD 0.000000
directive set -OPT_CONST_MULTS use_library
directive set -CHARACTERIZE_ROM false
directive set -PROTOTYPE_ROM true
directive set -ROM_THRESHOLD 64
directive set -CLUSTER_ADDTREE_IN_COUNT_THRESHOLD 0
directive set -CLUSTER_OPT_CONSTANT_INPUTS true
directive set -CLUSTER_RTL_SYN false
directive set -CLUSTER_FAST_MODE false
directive set -CLUSTER_TYPE combinational
directive set -COMPGRADE fast
directive set -DESIGN_HIERARCHY {top}
"""

libraries = """go analyze
solution library remove *
solution library add C28SOI_SC_12_CORE_LL_ccs -file {{$MGC_HOME/pkgs/siflibs/designcompiler/CORE65LPHVT_ccs.lib}} -- -rtlsyntool DesignCompiler -vendor STMicroelectronics -technology {{28nm FDSOI}}
solution library add ST_singleport_4096x128
solution library add ST_singleport_16384x32
solution library add ST_singleport_8192x32
go libraries
directive set -CLOCKS {{clk {{-CLOCK_PERIOD {period:.2f} -CLOCK_EDGE rising -CLOCK_HIGH_TIME {halfperiod:.2f} -CLOCK_OFFSET 0.000000 -CLOCK_UNCERTAINTY 0.0 -RESET_KIND sync -RESET_SYNC_NAME rst -RESET_SYNC_ACTIVE high -RESET_ASYNC_NAME arst_n -RESET_ASYNC_ACTIVE low -ENABLE_NAME {{}} -ENABLE_ACTIVE high}}}}
go assembly
directive set /{top}/core/main -PIPELINE_INIT_INTERVAL 1
directive set /{top}/core -CLOCK_OVERHEAD 0.0
"""

genCore = """directive set /{top}/core/doCore<0U>:core.REG:rsc/MAP_TO_MODULE {{[Register]}}
go architect
"""

genCacheCore = """directive set /{top}/cim:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
directive set /{top}/cim:rsc -INTERLEAVE {associativity}
directive set /{top}/cdm:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
directive set /{top}/cdm:rsc -INTERLEAVE {associativity}

directive set /{top}/memictrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
directive set /{top}/memictrl -WORD_WIDTH 128
directive set /{top}/memdctrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
directive set /{top}/memdctrl -WORD_WIDTH 128
go architect
//cycle add /{top}/core/core:rlp/main/loadidata:read_mem(cim:rsc(0)(0).@) -from /{top}/core/core:rlp/main/icache:case-0:if:setctrl:read_mem(memictrl:rsc.@) -equal 0
//cycle add /{top}/core/core:rlp/main/dcache:case-4:if:read_mem(cdm:rsc(0)(0).@) -from /{top}/core/core:rlp/main/dcache:case-0:if:setctrl:read_mem(memdctrl:rsc.@) -equal 0
//cycle add /{top}/core/core:rlp/main/dcache:case-0:if:setctrl:read_mem(memdctrl:rsc.@) -from /{top}/core/core:rlp/main/icache:case-0:if:setctrl:read_mem(memictrl:rsc.@) -equal 0
//if {{ {associativity} != 1 }} {{
set memconstraint [cycle find_op "*mem(cdm*" -all true]
set memconstraint [concat $memconstraint [cycle find_op "*mem(cim*" -all true]]
set memconstraint [concat $memconstraint [cycle find_op "*mem(mem*" -all true]]
for {{ set i 1 }} {{ $i < [llength $memconstraint] }} {{ incr i }} {{
	cycle add [lindex $memconstraint $i-1] -from [lindex $memconstraint $i] -equal 0
}}
//}}
"""



genCacheonly = """if {{ {cachesize} == 65536 }} {{
	directive set /{top}/cim:rsc -MAP_TO_MODULE ST_singleport_16384x32.ST_SPHD_BB_16384x32m32_aTdol_wrapper
	directive set /{top}/cim:rsc -INTERLEAVE {associativity}
	directive set /{top}/cdm:rsc -MAP_TO_MODULE ST_singleport_16384x32.ST_SPHD_BB_16384x32m32_aTdol_wrapper
	directive set /{top}/cdm:rsc -INTERLEAVE {associativity}
}} else {{
	directive set /{top}/cim:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
	directive set /{top}/cim:rsc -INTERLEAVE {associativity}
	directive set /{top}/cdm:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
	directive set /{top}/cdm:rsc -INTERLEAVE {associativity}
}}
directive set /{top}/memictrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
directive set /{top}/memictrl -WORD_WIDTH 128
directive set /{top}/memdctrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
directive set /{top}/memdctrl -WORD_WIDTH 128
go architect
//cycle add /{top}/core/core:rlp/main/loadidata:read_mem(cim:rsc(0)(0).@) -from /{top}/core/core:rlp/main/icache:case-0:if:setctrl:read_mem(memictrl:rsc.@) -equal 0
//cycle add /{top}/core/core:rlp/main/dcache:case-4:if:read_mem(cdm:rsc(0)(0).@) -from /{top}/core/core:rlp/main/dcache:case-0:if:setctrl:read_mem(memdctrl:rsc.@) -equal 0
//cycle add /{top}/core/core:rlp/main/dcache:case-0:if:setctrl:read_mem(memdctrl:rsc.@) -from /{top}/core/core:rlp/main/icache:case-0:if:setctrl:read_mem(memictrl:rsc.@) -equal 0
if {{ {associativity} != 1 }} {{
	set read_mem [cycle find_op *read_mem* -all true]
	set write_mem [cycle find_op *write_mem* -all true]
	for {{ set i 1 }} {{ $i < [llength $read_mem] }} {{ incr i }} {{
		cycle add [lindex $read_mem $i-1] -from [lindex $read_mem $i] -equal 0
	}}
	cycle add [lindex $read_mem $i-1] -from [lindex $write_mem 0] -equal 0
	for {{ set i 1 }} {{ $i < [llength $write_mem] }} {{ incr i }} {{
		cycle add [lindex $write_mem $i-1] -from [lindex $write_mem $i] -equal 0
	}}
}}
"""


constraintall = """set read_mem [cycle find_op *read_mem* -all true]
set write_mem [cycle find_op *write_mem* -all true]
while { [lsearch $read_mem "*memory*"] != -1 } {
	set idx [lsearch $read_mem "*memory*"]
	set read_mem [lreplace $read_mem $idx $idx]
}
while { [lsearch $write_mem "*memory*"] != -1 } {
	set idx [lsearch $write_mem "*memory*"]
	set write_mem [lreplace $write_mem $idx $idx]
}
foreach rm $read_mem {
	foreach wm $write_mem {
		cycle add $rm -from $wm -equal 0
	}
}
"""

endScript = """go schedule
go extract
project save {name}.ccs
set end [clock seconds]
puts "Done in [expr ($end-$start)/60]:[expr ($end-$start)%60]"
set blocksize {blocksize}
"""

exploreCache = """dofile func.tcl
set Associativity [list 2 4]
set BlockSize [list 2 4 8 16]
set CacheSize [list 1024 2048 4096 8192 16384 32768 65536]
set Policy [list 1 2 3]
set StrPolicy [list fifo lru random]
foreach cachesize $CacheSize {
	if { [catch {
		go new
		solution new -state initial
		solution options set /Input/CompilerFlags "-D __CATAPULT__=1 -DSize=$cachesize -DAssociativity=1 -DBlocksize=$blocksize -DPolicy=0"
		solution options set /Input/SearchPath ../include
		solution options set ComponentLibs/SearchPath ../memories -append
		flow package require /SCVerify
		solution file add ../src/core.cpp -type C++
		solution file add ../src/reformeddm_sim.cpp -type C++ -exclude True
		solution file add ../src/cache.cpp -type C++
		solution file add ../src/simulator.cpp -type C++ -exclude True
		solution file add ../src/elfFile.cpp -type C++ -exclude True
		solution file add ../src/portability.cpp -type C++
		solution rename "${cachesize}Direct1x[expr 4*$blocksize]"
		directive set -DESIGN_GOAL area
		directive set -OLD_SCHED false
		directive set -SPECULATE true
		directive set -MERGEABLE true
		directive set -REGISTER_THRESHOLD 4096
		directive set -MEM_MAP_THRESHOLD 64
		directive set -FSM_ENCODING none
		directive set -REG_MAX_FANOUT 0
		directive set -NO_X_ASSIGNMENTS true
		directive set -SAFE_FSM false
		directive set -ASSIGN_OVERHEAD 0
		directive set -UNROLL no
		directive set -IO_MODE super
		directive set -REGISTER_IDLE_SIGNAL false
		directive set -IDLE_SIGNAL {}
		directive set -STALL_FLAG false
		directive set -TRANSACTION_DONE_SIGNAL true
		directive set -DONE_FLAG {}
		directive set -READY_FLAG {}
		directive set -START_FLAG {}
		directive set -BLOCK_SYNC none
		directive set -TRANSACTION_SYNC ready
		directive set -DATA_SYNC none
		directive set -RESET_CLEARS_ALL_REGS true
		directive set -CLOCK_OVERHEAD 0.000000
		directive set -OPT_CONST_MULTS use_library
		directive set -CHARACTERIZE_ROM false
		directive set -PROTOTYPE_ROM true
		directive set -ROM_THRESHOLD 64
		directive set -CLUSTER_ADDTREE_IN_COUNT_THRESHOLD 0
		directive set -CLUSTER_OPT_CONSTANT_INPUTS true
		directive set -CLUSTER_RTL_SYN false
		directive set -CLUSTER_FAST_MODE false
		directive set -CLUSTER_TYPE combinational
		directive set -COMPGRADE fast
		directive set -DESIGN_HIERARCHY cacheWrapper
		"""+libraries.format(top="cacheWrapper", period=1.5, halfperiod=0.75)+"""
		go assembly
		directive set /cacheWrapper/cim:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
		directive set /cacheWrapper/cim:rsc -INTERLEAVE 1
		directive set /cacheWrapper/cdm:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
		directive set /cacheWrapper/cdm:rsc -INTERLEAVE 1
		directive set /cacheWrapper/memictrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
		directive set /cacheWrapper/memictrl -WORD_WIDTH 128
		directive set /cacheWrapper/memdctrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
		directive set /cacheWrapper/memdctrl -WORD_WIDTH 128
		go architect
		set read_mem [cycle find_op *read_mem* -all true]
		set write_mem [cycle find_op *write_mem* -all true]
		for { set i 1 } { $i < [llength $read_mem] } { incr i } {
			cycle add [lindex $read_mem $i-1] -from [lindex $read_mem $i] -equal 0
		}
		cycle add [lindex $read_mem $i-1] -from [lindex $write_mem 0] -equal 0
		for { set i 1 } { $i < [llength $write_mem] } { incr i } {
			cycle add [lindex $write_mem $i-1] -from [lindex $write_mem $i] -equal 0
		}
		go extract
	} ] }	{ continue }
}
foreach policy $Policy {
	foreach assoc $Associativity {
		foreach cachesize $CacheSize {
			if { [catch {
				go new
				solution new -state initial
				solution rename "${cachesize}[lindex $StrPolicy [lsearch $Policy $policy]]${assoc}x[expr 4*$blocksize]"
				solution options set /Input/CompilerFlags "-D __CATAPULT__=1 -DSize=$cachesize -DAssociativity=$assoc -DBlocksize=$blocksize -DPolicy=$policy"
				solution options set /Input/SearchPath ../include
				solution options set ComponentLibs/SearchPath ../memories -append
				flow package require /SCVerify
				solution file add ../src/core.cpp -type C++
				solution file add ../src/reformeddm_sim.cpp -type C++ -exclude True
				solution file add ../src/cache.cpp -type C++
				solution file add ../src/simulator.cpp -type C++ -exclude True
				solution file add ../src/elfFile.cpp -type C++ -exclude True
				solution file add ../src/portability.cpp -type C++
				directive set -DESIGN_GOAL area
				directive set -OLD_SCHED false
				directive set -SPECULATE true
				directive set -MERGEABLE true
				directive set -REGISTER_THRESHOLD 4096
				directive set -MEM_MAP_THRESHOLD 64
				directive set -FSM_ENCODING none
				directive set -REG_MAX_FANOUT 0
				directive set -NO_X_ASSIGNMENTS true
				directive set -SAFE_FSM false
				directive set -ASSIGN_OVERHEAD 0
				directive set -UNROLL no
				directive set -IO_MODE super
				directive set -REGISTER_IDLE_SIGNAL false
				directive set -IDLE_SIGNAL {}
				directive set -STALL_FLAG false
				directive set -TRANSACTION_DONE_SIGNAL true
				directive set -DONE_FLAG {}
				directive set -READY_FLAG {}
				directive set -START_FLAG {}
				directive set -BLOCK_SYNC none
				directive set -TRANSACTION_SYNC ready
				directive set -DATA_SYNC none
				directive set -RESET_CLEARS_ALL_REGS true
				directive set -CLOCK_OVERHEAD 0.000000
				directive set -OPT_CONST_MULTS use_library
				directive set -CHARACTERIZE_ROM false
				directive set -PROTOTYPE_ROM true
				directive set -ROM_THRESHOLD 64
				directive set -CLUSTER_ADDTREE_IN_COUNT_THRESHOLD 0
				directive set -CLUSTER_OPT_CONSTANT_INPUTS true
				directive set -CLUSTER_RTL_SYN false
				directive set -CLUSTER_FAST_MODE false
				directive set -CLUSTER_TYPE combinational
				directive set -COMPGRADE fast
				directive set -DESIGN_HIERARCHY cacheWrapper
				"""+libraries.format(top="cacheWrapper", period=1.5, halfperiod=0.75)+"""
				go assembly
				if { $cachesize == 65536 } {
					directive set /cacheWrapper/cim:rsc -MAP_TO_MODULE ST_singleport_16384x32.ST_SPHD_BB_16384x32m32_aTdol_wrapper
					directive set /cacheWrapper/cim:rsc -INTERLEAVE $assoc
					directive set /cacheWrapper/cdm:rsc -MAP_TO_MODULE ST_singleport_16384x32.ST_SPHD_BB_16384x32m32_aTdol_wrapper
					directive set /cacheWrapper/cdm:rsc -INTERLEAVE $assoc
				} else {
					directive set /cacheWrapper/cim:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
					directive set /cacheWrapper/cim:rsc -INTERLEAVE $assoc
					directive set /cacheWrapper/cdm:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
					directive set /cacheWrapper/cdm:rsc -INTERLEAVE $assoc
				}
				directive set /cacheWrapper/memictrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
				directive set /cacheWrapper/memictrl -WORD_WIDTH 128
				directive set /cacheWrapper/memdctrl:rsc -MAP_TO_MODULE ST_singleport_4096x128.ST_SPHD_BB_4096x128m8_aTdol_wrapper
				directive set /cacheWrapper/memdctrl -WORD_WIDTH 128
				go architect
				set read_mem [cycle find_op *read_mem* -all true]
				set write_mem [cycle find_op *write_mem* -all true]
				for { set i 1 } { $i < [llength $read_mem] } { incr i } {
					cycle add [lindex $read_mem $i-1] -from [lindex $read_mem $i] -equal 0
				}
				cycle add [lindex $read_mem $i-1] -from [lindex $write_mem 0] -equal 0
				for { set i 1 } { $i < [llength $write_mem] } { incr i } {
					cycle add [lindex $write_mem $i-1] -from [lindex $write_mem $i] -equal 0
				}
				go extract
			}]} { continue }
		}
	}
}
"""

exploreCore = """dofile func.tcl
set maxf 0
set co 0
for {set overhead 0.0} {$overhead < 20} {set overhead [expr $overhead+1]} {
	if { [catch {
		for {set f 700} {$f < 1000} {set f [expr $f+5]} {
			if { [catch {
				set period [string range [expr 1000./$f] 0 4]
				set solutionname "${f}MHz_${overhead}CO"
				go new
				solution new
				solution rename ${solutionname}
				directive set -CLOCK_OVERHEAD $overhead
				go libraries
				directive set -CLOCKS "clk {-CLOCK_PERIOD ${period} -CLOCK_EDGE rising -CLOCK_HIGH_TIME [expr ${period}/2] -CLOCK_OFFSET 0.000000 -CLOCK_UNCERTAINTY 0.0 -RESET_KIND sync -RESET_SYNC_NAME rst -RESET_SYNC_ACTIVE high -RESET_ASYNC_NAME arst_n -RESET_ASYNC_ACTIVE low -ENABLE_NAME {} -ENABLE_ACTIVE high}"
				go architect
				go schedule
				go extract
				if {$maxf < $f } {
					set maxf [max $f $maxf]
					set co $overhead
					logfile message "New maximum frequency achieved : $maxf\n" info
				}
			}
		]} {
			logfile message "Failed to synthesize for $f MHz with $overhead\% clock overhead\n" info
			puts "Failed to synthesize for $f MHz with $overhead\% clock overhead\n"
			break
		} }
	}]} {
		logfile message "Unexpected error\n" error
		puts "Unexpected error\n"
	}
}
logfile message "Maximum frequency : $maxf for $co\%\n" comment
puts "Maximum frequency : $maxf for $co\%\n"
project save
"""

def is_powerof2(num):
	return (num & (num - 1)) == 0 and num > 0

def nextpowerof2(num):
	assert(num > 0)
	return 2**(num-1).bit_length()
	
def doMem(memsize, bitwidth, name=None):
	area = int(1.4624*memsize*(bitwidth/8)+4915)
	memorypath = "../memories/ST_singleport_{}x{}.lib".format(memsize, bitwidth)
	if not os.path.isfile(memorypath):
		mem = genMem.format(**locals())
		if name is not None:
			print("Generating {} memory {}x{} with area {}".format(name, memsize, bitwidth, area))
		else:
			print("Generating memory {}x{} with area {}".format(memsize, bitwidth, area))
		with open("../memories/generateCacheMemories.tcl", "w") as f:
			f.write(mem)
		subprocess.check_call(["./catapult.sh", "-product lb -shell -f ../memories/generateCacheMemories.tcl"])

def doCore(doCache, cachesize, associativity, blocksize, policy, explore=False, name="", cacheonly=False):
	nocache = "" if doCache else "-Dnocache"
	sets = int(cachesize/(blocksize)/associativity)
	tagbits = int(32 - log2(blocksize/4) - log2(sets) - 2)
	bits = int(associativity*(tagbits+1+1)+associativity*(associativity-1)/2)
	if name == "":
		if cacheonly:
			name = str(cachesize) + policy + str(associativity) + "cache"
		else:
			name = str(cachesize)+"x32cachecore"
	
	policytodefine = {"NONE" : 0, "FIFO" : 1, "LRU" : 2, "RANDOM" : 3}
	policy = policytodefine[policy]
	
	if doCache:
		print("Generating", name, "with cachesize ", cachesize, " bytes and associativity ", associativity, " and block size of ", blocksize, " bytes.")
		print("Tagbits {}\nBitwidth of control {}({})\nSets {}".format(tagbits, bits, nextpowerof2(bits), sets))
	else:
		print("Generating", name, "without cache.")

	bitwidth = nextpowerof2(bits)
	sets = nextpowerof2(sets)
	area = int((1.4624*sets*bitwidth)/8+4915)
	tagbits = 4*tagbits

	# ~ doMem(2**24, 32, "main")
	# ~ if doCache:
		# ~ doMem(sets, bitwidth, "control")
		# ~ doMem(int(8*cachesize/32), 32, "data")
	bitwidth = 32

	#core
	datasize = int(8*cachesize/bitwidth)
	interleaveddatasize = int(8*cachesize/bitwidth/associativity)
	blocksize = int(blocksize/4)
	datawidth = 32
	ctrlwidth = nextpowerof2(bits)
	period = 1.5
	halfperiod = period/2
	
	if cacheonly:
		top = "cacheWrapper"
		core = (header + libraries + genCacheonly).format(**locals())
	elif doCache:
		top = "doStep"
		core = (header + libraries + genCacheCore).format(**locals())
	else:
		top = "doStep"
		core = (header + libraries + genCore).format(**locals())
		
	# ~ core += constraint
	
	core += endScript.format(**locals())
	
	if explore:
		if cacheonly:
			solutionCacheOnly = (header+libraries+genCacheonly+endScript).format(**locals())
			core += exploreCache
		else:
			core += exploreCore
		
	with open("{}.tcl".format(name), "w") as f:
		f.write(core)

	if args.shell:
		subprocess.check_call(["./catapult.sh", "-shell -f {}.tcl".format(name)], stdout=subprocess.DEVNULL)
	else:
		subprocess.check_call(["./catapult.sh", "-f {}.tcl".format(name)])

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument("name", help="name of the project to be generated", default="", nargs='?')
	parser.add_argument("-s", "--shell", help="Launch catapult in shell", action="store_true")
	parser.add_argument("-n", "--no-cache", help="Synthesize without cache", action="store_true")
	parser.add_argument("-c", "--cache-size", help="Cache size in bytes", type=int)
	parser.add_argument("-a", "--associativity", help="Cache associativity", type=int)
	parser.add_argument("-b", "--blocksize", help="Cache blocksize in bytes", type=int)
	parser.add_argument("-p", "--policy", help="Replacement policy")
	parser.add_argument("-e", "--explore", help="Do some exploration in the solution space", action="store_true")
	parser.add_argument("-o", "--cache-only", help="Generate only cache", action="store_true")

	args = parser.parse_args()
	nocache = args.no_cache
	try:
		cachesize = args.cache_size
		assert is_powerof2(cachesize), "cachesize is not a power of 2"
	except TypeError:
		cachesize = 1024
		print("Setting default cachesize to {}".format(cachesize))

	try:
		associativity = args.associativity
		assert is_powerof2(associativity), "associativity is not a power of 2"
	except TypeError:
		associativity = 4
		print("Setting default associativity to {}".format(associativity))

	try:
		blocksize = int(args.blocksize/1)
		assert is_powerof2(blocksize), "blocksize is not a power of 2"
	except TypeError:
		blocksize = 32
		print("Setting default blocksize to {}".format(blocksize))

	try:
		policy = args.policy + ""
	except TypeError:
		if associativity == 1:
			policy = "NONE"
		else:
			policy = "LRU"
		print("Setting default policy to {}".format(policy))

	policy = policy.upper()
	assert (associativity == 1 and policy == "NONE") or (associativity != 1 and policy != "NONE")
	assert policy in ["LRU", "RANDOM", "NONE", "FIFO"]
	
	assert (args.cache_only and args.no_cache) is False, "Cannot have nocache with cache-only option"

	# ~ defines = "-D{}={} "
	# ~ defines = defines.format("Size", cachesize) + defines.format("Associativity", associativity) + defines.format("Blocksize", int(blocksize/4)) + defines.format("Policy", policy)
	# ~ subprocess.check_call(["make", "DEFINES="+defines])
	# ~ with open("output.log", "w") as output:
		# ~ subprocess.check_call(["./testbench.sim"], stdout=output)

	if args.explore and args.cache_only:
		processes = []
		args.shell = True
		for blocksize in (8,16,32,64):
			processes.append(Process(target=doCore, args=(not nocache, cachesize, associativity, blocksize, policy, args.explore, "explCache{}".format(blocksize), args.cache_only)))
			processes[-1].start()
		
		while any(p.join() for p in processes):
			sleep(1)
	else:
		doCore(not nocache, cachesize, associativity, blocksize, policy, args.explore, args.name, args.cache_only)
	
