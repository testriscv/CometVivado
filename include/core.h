#ifndef CORE_H
#define CORE_H

#include "portability.h"
#include "riscvISA.h"
#include "simulator.h"

struct FtoDC
{
    FtoDC()
    : pc(0), instruction(0x13), realInstruction(false), nextpc(0)
    {}
    ac_int<32, false> pc;           // used for JAL, AUIPC & BR
    ac_int<32, false> instruction;  // Instruction to execute
    bool realInstruction;           // Increment for minstret
    ac_int<32, false> nextpc;       // Next pc to store for JAL & JALR
};

struct DCtoEx
{
    DCtoEx()
    : pc(0),
  #ifndef __HLS__
      instruction(0),
  #endif
      opCode(RISCV_OPI), funct7(0), funct3(RISCV_OPI_ADDI), rd(0), realInstruction(false),
      lhs(0), rhs(0), datac(0), forward_lhs(false), forward_rhs(false), forward_datac(false),
      forward_mem_lhs(false), forward_mem_rhs(false), forward_mem_datac(false)
  #ifndef __HLS__
      , datad(0), datae(0), memValue(0)
  #endif
    {}

    ac_int<32, false> pc;       // used for branch
#ifndef __HLS__
    ac_int<32, false> instruction;
#endif

    ac_int<7, false> opCode;    // opCode = instruction[6:0]
    ac_int<7, false> funct7;    // funct7 = instruction[31:25]
    ac_int<3, false> funct3;    // funct3 = instruction[14:12]
 // ac_int<5, false> rs1;       // rs1    = instruction[19:15]
 // ac_int<5, false> rs2;       // rs2    = instruction[24:20]
    ac_int<5, false> rd;        // rd     = instruction[11:7]

    bool realInstruction;

    ac_int<32, true> lhs;   //  left hand side : operand 1
    ac_int<32, true> rhs;   // right hand side : operand 2
    ac_int<32, true> datac; // ST, BR, JAL/R,

    bool forward_lhs;
    bool forward_rhs;
    bool forward_datac;
    bool forward_mem_lhs;
    bool forward_mem_rhs;
    bool forward_mem_datac;

#ifndef __HLS__
    // syscall only
    ac_int<32, true> datad;
    ac_int<32, true> datae;
    ac_int<32, true> memValue; //Second data, from register file or immediate value
#endif
};

struct ExtoMem
{
    ExtoMem()
    : pc(0),
  #ifndef __HLS__
      instruction(0x13),
  #endif
      result(0), rd(0), opCode(RISCV_OPI), funct3(RISCV_OPI_ADDI), realInstruction(false),
      datac(0)
    {}
    ac_int<32, false> pc;
#ifndef __HLS__
    ac_int<32, false> instruction;
#endif

    ac_int<32, true> result;    // result of the EX stage
    ac_int<5, false> rd;        // destination register
    ac_int<7, false> opCode;    // LD or ST (can be reduced to 2 bits)
    ac_int<3, false> funct3;    // datasize and sign extension bit
    bool realInstruction;

    ac_int<32, true> datac;     // data to be stored in memory (if needed)
};

struct MemtoWB
{
    MemtoWB()
    : pc(0),
  #ifndef __HLS__
      instruction(0x13),
  #endif
      result(0), rd(0), realInstruction(false), rescsr(0), CSRid(0), csrwb(false)
    {}
    ac_int<32, false> pc;
#ifndef __HLS__
    ac_int<32, false> instruction;
#endif

    ac_int<32, true> result;    // Result to be written back
    ac_int<5, false> rd;        // destination register
    bool realInstruction;       // increment minstret ?

    ac_int<32, false> rescsr;   // Result for CSR instruction
    ac_int<12, false> CSRid;    // CSR to be written back
    bool csrwb;
};

struct CSR
{
    CSR()
    : mcycle(0), minstret(0), mstatus(0)
    // some should probably be initialized to some special value
    {}
    static const ac_int<32, false> mvendorid;   // RO shared by all cores
    static const ac_int<32, false> marchid;     // RO shared by all cores
    static const ac_int<32, false> mimpid;      // RO shared by all cores
    //const ac_int<32, false> mhartid;                  // RO but private to core (and i don't want to template everything)
    ac_int<64, false> mcycle;                   // could be shared according to specification
    ac_int<64, false> minstret;
    ac_int<32, false> mstatus;
    ac_int<32, false> misa;     // writable...
    ac_int<32, false> medeleg;
    ac_int<32, false> mideleg;
    ac_int<32, false> mie;
    ac_int<32, false> mtvec;
    ac_int<32, false> mcounteren;
    ac_int<32, false> mscratch;
    ac_int<32, false> mepc;
    ac_int<32, false> mcause;
    ac_int<32, false> mtval;
    ac_int<32, false> mip;
};

struct CoreCtrl
{
    CoreCtrl()
    : lock(0), freeze_fetch(false), cachelock(false), init(false), sleep(true)
    {
        #pragma hls_unroll yes
        for(int i(0); i < 3; ++i)
        {
            prev_rds[i] = 0;
            prev_opCode[i] = RISCV_OPI;
            prev_res[i] = 0;
            branch[i] = false;
        }
        #pragma hls_unroll yes
        for(int i(0); i < 2; ++i)
        {
            jump_pc[i] = 0;
        }
    }

    ac_int<5, false> prev_rds[3];
    ac_int<7, false> prev_opCode[3];
    ac_int<2, false> lock;          // used to lock dc stage after JAL & JALR

    bool freeze_fetch;              // used for LD dependencies
    bool cachelock;                 // stall Ft, DC & Ex when cache is working
    bool init;                      // is core initialized?
    bool sleep;                     // sleep core, can be waken up by other core

    // used to break dependencies, because using extoMem or memtoWB
    // implies a dependency from stage ex or mem to dc (i.e. they
    // are not completely independent)...
    ac_int<32, true> prev_res[3];
    bool branch[3];
    ac_int<32, true> jump_pc[2];
};

struct Core
{
    Core()
    : ftoDC(), dctoEx(), extoMem(), memtoWB(), csrs(), ctrl(), pc(0),
      irequest(), ireply(), drequest(), dreply()
    {
        #pragma hls_unroll yes
        for(int i(0); i < 32; ++i)
        {
            REG[i] = 0;
        }
        REG[2] = STACK_INIT;
    }

    FtoDC ftoDC;
    DCtoEx dctoEx;
    ExtoMem extoMem;
    MemtoWB memtoWB;
    CSR csrs;

    CoreCtrl ctrl;

    ac_int<32, true> REG[32];
    ac_int<32, false> pc;

    /// Instruction cache
    //unsigned int idata[Sets][Blocksize][Associativity];   // made external for modelsim
    ICacheRequest irequest;
    ICacheReply ireply;

    /// Data cache
    //unsigned int ddata[Sets][Blocksize][Associativity];   // made external for modelsim
    DCacheRequest drequest;
    DCacheReply dreply;
};

class Simulator;

void doStep(ac_int<32, false> startpc, bool &exit,
            unsigned int im[DRAM_SIZE], unsigned int dm[DRAM_SIZE],
            unsigned int cim[Sets][Blocksize][Associativity], unsigned int cdm[Sets][Blocksize][Associativity],
            ac_int<IWidth, false> memictrl[Sets], ac_int<DWidth, false> memdctrl[Sets]
        #ifndef __HLS__
            , Simulator* syscall
        #endif
            );


#endif  // CORE_H
