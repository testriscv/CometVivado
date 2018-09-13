#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <map>

struct Core;

#include "portability.h"
#include "cache.h"

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

    //********************************************************
    //Instrumentation

    struct CacheInstrumentation
    {
        CacheInstrumentation()
        : miss(0), hit(0), cachememread(0), cachememwrite(0), mainmemread(0),
          mainmemwrite(0), ctrlmemread(0), ctrlmemwrite(0)
        {}
        int64_t miss;
        int64_t hit;
        int64_t cachememread;
        int64_t cachememwrite;
        int64_t mainmemread;
        int64_t mainmemwrite;
        int64_t ctrlmemread;
        int64_t ctrlmemwrite;
    } icachedata, dcachedata;

    struct CoreInstrumentation
    {
        CoreInstrumentation()
        {
            int64_t* ptr = &lui;
            for(int i(0); i < sizeof(CoreInstrumentation)/8; ++i)
                ptr[i] = 0;
        }
        int64_t lui;
        int64_t auipc;
        int64_t jal;
        int64_t jalr;
        int64_t br[6];
        int64_t ld;
        int64_t st;
        int64_t addi;
        int64_t slti;
        int64_t sltiu;
        int64_t xori;
        int64_t ori;
        int64_t andi;
        int64_t slli;
        int64_t srai;
        int64_t srli;
        int64_t mul[4];
        int64_t div[2];
        int64_t rem[2];
        int64_t add;
        int64_t sub;
        int64_t sll;
        int64_t slt;
        int64_t sltu;
        int64_t opxor;  // xor is c++ keyword
        int64_t opor;   // or  is c++ keyword
        int64_t opand;  // and is c++ keyword
        int64_t sra;
        int64_t srl;
        int64_t misc_mem;
        int64_t ecall;
        int64_t csrrw;
        int64_t csrrs;
        int64_t csrrc;
        int64_t csrrwi;
        int64_t csrrsi;
        int64_t csrrci;

        int64_t bulle;

        int64_t total()
        {
            int64_t* ptr = &lui;
            int64_t t = 0;
            for(int i(0); i < sizeof(CoreInstrumentation)/8; ++i)
                t += ptr[i];
            return t-bulle;
        }
    } coredata;
};

#endif // SIMULATOR_H
