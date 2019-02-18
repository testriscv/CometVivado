# Comet
RISC-V 32-bit processor written in C++ for High Level Synthesis (HLS).
RV32I base ISA, support for M extension

## Comet RTL files

- [Latest ASIC RTL](https://gitlab.inria.fr/srokicki/comet/-/jobs/artifacts/rework/browse?job=catapult_ASIC)
- [Latest Xilinx RTL](https://gitlab.inria.fr/srokicki/comet/-/jobs/artifacts/rework/browse?job=catapult_Xilinx)

## Dependencies
To compile the simulator:
  - cmake

To compile the benchmarks:
  - [RISC-V GNU toolchain](https://github.com/riscv/riscv-tools)

## Building the simulator

Once the repository cloned, `cd` into it and run:

```
mkdir build && cd build
cmake ..
make
```

The simulator binary is located in `<repo_root>/build/bin` under the name `comet.sim`

## Building the benchmarks

This repository includes a basic set of benchmarks (`dijkstra`, `matmul` and `qsort`) working on different datasets.
To build them a working GNU RISC-V toolchain is required.

#### Building the GNU RISC-V toolchain

```
git clone --recursive https://github.com/riscv/riscv-gnu-toolchain
export RISCV=/path/to/install/riscv/toolchain
cd riscv-gnu-toolchain
```

The toolchain is dependant on a few packages that can be installed using

```
sudo apt-get install autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev
```
on a Debian based system.
Or using
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

#### Actually building the benchmarks

```
cd <repo_root>/benchmarks
make
```
The benchmark binaries will be located in `<repo_root>/benchmarks/build`.
There will be RISC-V binaries (having the `.riscv32` extension) as well as native (not cross compiled) ones for testing purposes.

## Simulator
To run the simulator execute the `comet.sim` in `<repo_root>/build/bin`.
By default the simulator will execute the `matmul_int_4.riscv32` benchmark. You can provide a specific binary to be run by the simulator using the `-f <path_to_the_binary>` switch.

The `-i` and `-o` switches are used to specify the benchmark's standard input and output respectively.

The `--` switch allows to pass arguments to the benchmark that is being run by the simulator.
