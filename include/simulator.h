#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <map>

struct Core;

#include "portability.h"
#include "cache.h"
#include "core.h"
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

    unsigned int* im;
    unsigned int* dm;

    FILE* input;
    FILE* output;
    unsigned int heapAddress;

    ac_int<DWidth, false>* dctrl;
    unsigned int* ddata; //[Sets][Blocksize][Associativity];
    Core* core;

public:
    Simulator(const char* binaryFile, const char* inputFile, const char* outputFile, int argc, char** argv);
    ~Simulator();

    void fillMemory();
    void setInstructionMemory(ac_int<32, false> addr, ac_int<8, true> value);
    void setDataMemory(ac_int<32, false> addr, ac_int<8, true> value);
    void setDM(unsigned int* d);
    void setIM(unsigned int* i);
    void setCore(Core* core, ac_int<DWidth, false>* dctrl, unsigned int cachedata[Sets][Blocksize][Associativity]);
    void setCore(Core* core);
    void writeBack();

    ac_int<32, true>* getInstructionMemory() const;
    ac_int<32, true>* getDataMemory() const;
    Core* getCore() const;

    void setPC(ac_int<32, false> pc);
    ac_int<32, false> getPC() const;

    //********************************************************
    //Memory interfaces

    void stb(ac_int<32, false> addr, ac_int<8, true> value);
    void sth(ac_int<32, false> addr, ac_int<16, true> value);
    void stw(ac_int<32, false> addr, ac_int<32, true> value);
    void std(ac_int<32, false> addr, ac_int<64, true> value);

    ac_int<8, true> ldb(ac_int<32, false> addr);
    ac_int<16, true> ldh(ac_int<32, false> addr);
    ac_int<32, true> ldw(ac_int<32, false> addr);
    ac_int<32, true> ldd(ac_int<32, false> addr);

    //********************************************************
    //System calls

    ac_int<32, true> solveSyscall(ac_int<32, true> syscallId, ac_int<32, true> arg1, ac_int<32, true> arg2, ac_int<32, true> arg3, ac_int<32, true> arg4, bool& sys_status);

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

};

#endif // SIMULATOR_H
