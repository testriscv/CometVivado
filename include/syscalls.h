/*
 * genericSimulator.h
 *
 *  Created on: 25 avr. 2017
 *      Author: simon
 */

#ifndef INCLUDES_SIMULATOR_GENERICSIMULATOR_H_
#define INCLUDES_SIMULATOR_GENERICSIMULATOR_H_

#include <map>
#include <ac_int.h>
#include "riscvISA.h"

/*********************************************************
 * 	Definition of the GenericSimulator class
 *
 * 	This class will contain all methods common to all ISA
 * 	simulators. It includes memory accesses and syscalls
 *
 *********************************************************/

class GenericSimulator {
public:

GenericSimulator(unsigned int* dm)
    : memory(dm)
{
    debugLevel = stop = nbInStreams = nbOutStreams = 0;
    heapAddress = 0;
}

int debugLevel;
int stop;

unsigned long long cycle;

unsigned int* memory;
ac_int<32, true> REG[32];
float regf[32];
void initialize(int argc, char* argv[]);


//********************************************************
//Memory interfaces

void stb(ac_int<32, false> addr, ac_int<8, true> value);
void sth(ac_int<32, false> addr, ac_int<16, true> value);
void stw(ac_int<32, false> addr, ac_int<32, true> value);
void std(ac_int<32, false> addr, ac_int<32, true> value);

ac_int<8, true> ldb(ac_int<32, false> addr);
ac_int<16, true> ldh(ac_int<32, false> addr);
ac_int<32, true> ldw(ac_int<32, false> addr);
ac_int<32, true> ldd(ac_int<32, false> addr);

//********************************************************
//System calls
unsigned long long profilingStarts[3];
unsigned long long profilingDomains[3];
std::map<ac_int<16, true>, FILE*> fileMap;
FILE **inStreams, **outStreams;
int nbInStreams, nbOutStreams;
unsigned int heapAddress;

ac_int<32, true> solveSyscall(ac_int<32, true> syscallId, ac_int<32, true> arg1, ac_int<32, true> arg2, ac_int<32, true> arg3, ac_int<32, true> arg4, ac_int<2, false>& sys_status);

ac_int<32, false> doRead(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size);
ac_int<32, false> doWrite(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size);
ac_int<32, false> doOpen(ac_int<32, false> name, ac_int<32, false> flags, ac_int<32, false> mode);
ac_int<32, false> doOpenat(ac_int<32, false> dir, ac_int<32, false> name, ac_int<32, false> flags, ac_int<32, false> mode);
ac_int<32, true> doLseek(ac_int<32, false> file, ac_int<32, false> ptr, ac_int<32, false> dir);
ac_int<32, false> doClose(ac_int<32, false> file);
ac_int<32, false> doStat(ac_int<32, false> filename, ac_int<32, false> ptr);
ac_int<32, false> doSbrk(ac_int<32, false> value);
ac_int<32, false> doGettimeofday(ac_int<32, false> timeValPtr);
ac_int<32, false> doUnlink(ac_int<32, false> path);

};

#endif /* INCLUDES_SIMULATOR_GENERICSIMULATOR_H_ */
