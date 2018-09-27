# Comet
RISC-V ISA based 32-bit processor written in C++ for High Level Synthesis (HLS).

## Pre-requisites
1. [RISC-V toolchain](https://github.com/riscv/riscv-tools)
2. [Catapult HLS](https://www.mentor.com/hls-lp/catapult-high-level-synthesis/)
3. [Python 3](https://www.python.org/downloads/)

## Components of the project
Vivado HLS is used for FPGA IP synthesis and Catapult HLS is used for ASIC synthesis.

* Run `make` to generate the simulator `comet.sim`.
* To build it as an FPGA IP, run `script.tcl` in Vivado HLS. (not maintained)
* To synthesize it to rtl for ASIC, run [genCore.py](catapult/genCore.py) in `catapult` folder. See `genCore.py -h` for argument list.

## Documentation 
The primary design goal behind using High Level Synthesis (HLS) is that for simulating the functionality on a host machine,
FPGA synthesis of ASIC synthesis the same C / C++ code can be used, with minimal changes. However since the target platforms,
and tools involved are different in nature, these differences have to be accounted for without having to re-write the major
functionality.

## Architecture
The 5 stage pipelined architecture currently has 64MB of instruction memory and 64MB of data memory. 32 bit
Arithmetic and logical integer operations are supported. We support M extension but the division and the modulo operation are external to the core because they take more than one cycle to execute at the synthesis. 

## Simulator
In order to execute an elf file compiled by `riscv32-unknown-elf-gcc`, it is necessary to have a process which reads the 
different sections of the binary file, extracts the program instructions and data, and stores it in the instruction and data 
memory. The functions to handle the the elf file are in [elfFile.cpp](src/elfFile.cpp), [elf.h](include/elf.h) and 
[elfFile.h](include/elfFile.h). The process which calls these functions, fills the memory and starts the execution of the 
processor, is defined in [reformeddm_sim.cpp](src/reformeddm_sim.cpp). 
To verify the simulator behavior, you can run a benchmark through it and check that the output is coherent with the [check.py](check.py) python script.

## Benchmarks
```bash
git clone --recursive https://github.com/riscv/riscv-tools 
export RISCV=/where/you/want/the/compilation/to/install
cd riscv-tools
./build-rv32ima.sh
export PATH=$PATH:$RISCV/bin
```
Now that you have compiled the compiler, you can compile the benchmarks:
```bash
cd benchmarks
make
```
After this, you can `./comet.sim -f benchmarks/build/matmul_int_4.riscv32` and this will run the benchmark through the simulator.

## Debug

You can compile with debug mode with `make debug` and this will enable all the debug output in the simulator. The output starts with a header that contains the cache parameters used in the simulation, then it presents the binary simulated followed by the address boundaries for each section in hexadecimal format.
```
Simulating benchmarks/build/matmul_int_4.riscv32
filling instruction from 010074 to 020943
filling data from 020948 to 021623
filling data from 021624 to 021663
filling data from 022000 to 022003
filling data from 022004 to 022007
```

Next part is a dump of the instruction memory where first line is `instruction memory :` and next lines are `address : instruction` where both address and instruction are in hexadecimal format. Instruction is always 4 bytes wide. Next part is a dump of the data memory where first line is `data memory :` and next lines are `address : datum (value)`.  Address and datum are hexadecimal, while value is decimal representation of datum. Datum is 1 byte. Both dump (instruction & data) include only non zero data. Missing addresses are null instruction or data. At the end, the line `end of preambule` indicates that the dump is finished.

Following part is the dump of the pipeline information for each cycle. First line is WriteBack stage that is writing back instruction `@address instruction (numinstr)`. Second line is `cycle regid:regvalue` where only non null registers are printed. Register value is the value **AFTER** the writeback and in hexadecimal format. Then we have `Mem @address instruction`, `Ex @address instruction`, `Dc @address instruction`, `Ft @address instruction`. If the stage is empty(bubble), only the stage name is printed. If Mem is executing a store or a load, Ex, Dc and Ft may not be printed because they are locked. The number before Ft is the address of the next requested instruction. `i @address instruction set way` may follow Ft indicating that the instruction cache has the requested instruction. The instruction address before and after Ft may not match because of a jump for instance.
```
WB   @01c7b8   01089513   (6931)
49976 1:00011a58 2:03ffedc0 3:00023228 5:000104d0 6:0000001c 9:03ffef5f 12:0000a000 13:0002152e 15:60000000 17:a0000000 18:000223dc 19:00022088 20:00000001 22:ffffffff 23:00000009 25:00000006 26:00000064 27:03ffef5f 
Mem  @01c7bc   01055513
Ex   
Dc   @01c7c0   0107d693
01c7c8
Ft   @01c7c4   02c85833
i    @01c7c8   01071713    6 0
```

In case of a memory access, the dump will look like this :
```
WB   @011a64   001a0a13   (6955)
50027 1:00011a58 2:03ffedc0 3:00023228 5:000104d0 6:0000001c 9:03ffef5e 10:00000036 12:0000a000 14:00006000 15:60000000 17:a0000000 18:000223dc 19:00022088 20:00000002 22:ffffffff 23:00000009 25:00000006 26:00000064 27:03ffef5f 
Mem  @011a68   00c12883
dR2  @3ffedcc   03ffeebc   03ffeebc    true   6 0
i    @011a74   0007c783    3 1
```
After Mem, we have `d(R|W)size @address  memvalue formatted_data signextension   set way`, other stages are not printed because they are locked. For write, signextension will not be shown. 
The dump may present other information such as external operation(DIV & REM) or some cache information (fetch/writeback).
At the end of the simulation, the line `Successfully executed 18050 instructions in 153793 cycles` should be present. Next is a dump of the data memory with the same format as the dump at the start of the simulation ended by `End of memory`. Then we have core statistics as well as cache statistics.

