#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <map>

#include <ac_int.h>

#include "portability.h"
#include "cache.h"
#include "elfFile.h"
#include "riscvISA.h"

class Simulator
{
private:
    //counters
    std::map<ac_int<32, false>, ac_int<8, false> > ins_memorymap;
    std::map<ac_int<32, false>, ac_int<8, false> > data_memorymap;

    ac_int<32, false> pc;

    ac_int<32, true>* ins_memory;
    ac_int<32, true>* data_memory;

    ac_int<32, true>* reg;
    unsigned int* im;
    unsigned int* dm;

    std::map<ac_int<16, true>, FILE*> fileMap;
    FILE* input;
    FILE* output;
    unsigned int heapAddress;

    DCacheControl* dctrl;
    unsigned int* ddata; //[Sets][Blocksize][Associativity];

public:
    Simulator(const char* binaryFile, const char* inputFile, const char* outputFile, int argc, char** argv);
    ~Simulator();

    void fillMemory();
    void setInstructionMemory(ac_int<32, false> addr, ac_int<8, true> value);
    void setDataMemory(ac_int<32, false> addr, ac_int<8, true> value);
    void setDM(unsigned int* d);
    void setIM(unsigned int* i);
    void setCore(ac_int<32, true>* r, DCacheControl* ctrl, unsigned int cachedata[Sets][Blocksize][Associativity]);

    ac_int<32, true>* getInstructionMemory() const;
    ac_int<32, true>* getDataMemory() const;

    void setPC(ac_int<32, false> pc);
    ac_int<32, false> getPC() const;

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

    ac_int<32, true> solveSyscall(ac_int<32, true> syscallId, ac_int<32, true> arg1, ac_int<32, true> arg2, ac_int<32, true> arg3, ac_int<32, true> arg4, bool& sys_status);

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

#endif // SIMULATOR_H
