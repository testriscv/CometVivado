#ifndef __BASIC_SIMULATOR_H__
#define __BASIC_SIMULATOR_H__

#include <map>
#include "simulator.h"

class BasicSimulator : public Simulator 
{
    unsigned heapAddress;
    std::map<ac_int<32, false>, ac_int<8, false> > imemMap;  
    std::map<ac_int<32, false>, ac_int<8, false> > dmemMap;  
    
    FILE* input;
    FILE* output;
public:
    BasicSimulator(char* binaryFile, int argc, char** argv, char* inputFile, char* outputFile); 
    ~BasicSimulator();

protected:
    void fillMemory();

    void printCycle(){ }//printf("pc: @%06x\n",core.pc); }; 
    void printStat(){};
    void extend(){};
    void solveSyscall();

    void insertInstructionMemoryMap(ac_int<32, false> addr, ac_int<8, false> value);
    void insertDataMemoryMap(ac_int<32, false> addr, ac_int<8, false> value);

    ac_int<32, true> doRead(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size);
    ac_int<32, true> doWrite(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size);
    ac_int<32, true> doOpen(ac_int<32, false> name, ac_int<32, false> flags, ac_int<32, false> mode);
    ac_int<32, true> doOpenat(ac_int<32, false> dir, ac_int<32, false> name, ac_int<32, false> flags, ac_int<32, false> mode);
    ac_int<32, true> doLseek(ac_int<32, false> file, ac_int<32, false> ptr, ac_int<32, false> dir);
    ac_int<32, true> doClose(ac_int<32, false> file);
    ac_int<32, true> doStat(ac_int<32, false> filename, ac_int<32, false> ptr);
    ac_int<32, true> doSbrk(ac_int<32, false> value);
    ac_int<32, true> doGettimeofday(ac_int<32, false> timeValPtr);
    ac_int<32, true> doUnlink(ac_int<32, false> path);
    ac_int<32, true> doFstat(ac_int<32, false> file, ac_int<32, false> stataddr);
    
    void stb(ac_int<32, false> addr, ac_int<8, true> value);
    void sth(ac_int<32, false> addr, ac_int<16, true> value);
    void stw(ac_int<32, false> addr, ac_int<32, true> value);
    void std(ac_int<32, false> addr, ac_int<64, true> value);

    ac_int<8, true> ldb(ac_int<32, false> addr);
    ac_int<16, true> ldh(ac_int<32, false> addr);
    ac_int<32, true> ldw(ac_int<32, false> addr);
    ac_int<32, true> ldd(ac_int<32, false> addr);
};

#endif // __BASIC_SIMULATOR_H__
