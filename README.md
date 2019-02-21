# Comet

[![pipeline status](https://gitlab.inria.fr/srokicki/Comet/badges/master/pipeline.svg)](https://gitlab.inria.fr/srokicki/Comet/commits/rework)


RISC-V 32-bit processor written in C++ for High Level Synthesis (HLS).
RV32I base ISA, support for M extension

## Comet RTL files

The latest working RTL sources of comet are downloadable for ASIC or FPGA targets following those two links :

- [Latest ASIC RTL](https://gitlab.inria.fr/srokicki/comet/-/jobs/artifacts/rework/browse?job=catapult_ASIC)
- [Latest Xilinx RTL](https://gitlab.inria.fr/srokicki/comet/-/jobs/artifacts/rework/browse?job=catapult_Xilinx)

## Dependencies
To compile the simulator:
  - cmake
  - the boost program_options library

To compile the tests:
  - [RISC-V GNU toolchain](https://github.com/riscv/riscv-tools)

## Building the simulator

Once the repository cloned, `cd` into it.

Make sure that all the dependencies are met by installing `cmake` and the `program_options` library from boost using your package manager or directly from the sources.

For exemple on a debian based system, run `sudo apt install cmake libboost-program-options`.

```
mkdir build && cd build
cmake ..
make
```

The simulator binary is located in `<repo_root>/build/bin` under the name `comet.sim`

#### Building the GNU RISC-V toolchain

To produce binaries that can be executed by Comet, a working 32bit RISC-V toolchain and libraries are needed.
To build a such a set of tools, these steps can be followed :

```
git clone --recursive https://github.com/riscv/riscv-gnu-toolchain
export RISCV=/path/to/install/riscv/toolchain
cd riscv-gnu-toolchain
```

The toolchain is dependant on a few packages that can be installed using :

```
sudo apt-get install autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev
```
on a Debian based system.
Or using :
```
sudo yum install autoconf automake libmpc-devel mpfr-devel gmp-devel gawk  bison flex texinfo patchutils gcc gcc-c++ zlib-devel expat-devel
```
on Fedora/CentOS/RHEL OS systems

Comet implements the 32bit ISA with the M extension (hardware multiply) and has no hardware floating point support.
The build process needs to be configured accordingly.

```
./configure --prefix=$RISCV --with-arch=rv32im --with-abi=ilp32
make
```

Once the toolchain compiled it is a good idea to add it's installation directory to the your system's PATH :
```
export PATH = $PATH:$RISCV
```

> You may want to add that command to your shell startup file

#### Building the tests

This repository includes a basic set of benchmarks (`dijkstra`, `matmul`, `qsort` and `dct`) working on different datatypes.

```
cd <repo_root>/tests
make
```
The `make` command will compile the tests for a RISC-V target as well for your system and will collect their expected output.
This will allow the `runTests.sh` script to check the behavior of Comet against a verified CPU (your system's).

##### Adding a test

To add a test to the test pool, add a folder in the `tests` folder at the root of the repository. This folder needs to named like the binary it will contain.

> Exemple : the `qsort` folder contains the `qsort.riscv32` binary

The folder must contain the sources of the program and a makefile.
The binary produced by the makefile must be named like the folder it's contained in and bear the .riscv32 extension.

For the test to be valid, a file named `expectedOutput` needs to be created. It must contain the standard output that the test is supposed to produce.

> You can reuse the makefile of the existing tests as a starting point.
> This makefile compiles the sources for the RISC-V target as well as for the host system, runs the native binary and creates the `expectedOutput` file automatically

All the tests **must** be reproducible output-wise, no random or system dependent data should be used.

The `runTests.sh` script executes all the tests present in the `tests` subfolders with a timeout (set in the script).
Please make sure that the tests you add all execute within the bounds of this timeout with a reasonable margin.

## Simulator
To run the simulator execute the `comet.sim` in `<repo_root>/build/bin`.
You can provide a specific binary to be run by the simulator using the `-f <path_to_the_binary>` switch.

The `-i` and `-o` switches are used to specify the benchmark's standard input and output respectively.

The `-a` switch allows to pass arguments to the benchmark that is being run by the simulator.

For further information about the arguments of the simulator, run `comet.sim -h`.

## Todo

- Fix artifacts lifetime once the version allows it (or make something that pushes automatically every 200 years)
- Add CSR support
- Add a linter to the project
