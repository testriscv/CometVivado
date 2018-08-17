#ifndef CORE_H
#define CORE_H

#include "portability.h"
#include "simulator.h"

struct FtoDC
{
    ac_int<32, false> pc;           // used for JAL, AUIPC & BR
    ac_int<32, false> instruction;  // Instruction to execute
    bool realInstruction;           // Increment for minstret
    ac_int<32, false> nextpc;       // Next pc to store for JAL & JALR
};

struct DCtoEx
{
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

namespace ExternalOperation
{
enum ExternalOperation
{
    DIV     ,
    DIVU    ,
    REM     ,
    REMU
};
}

struct MultiCycleOp
{
    ExternalOperation::ExternalOperation op;
    ac_int<32, true> lhs;
    ac_int<32, true> rhs;
    ac_int<5, false> rd;
};

struct MultiCycleRes
{
    ExternalOperation::ExternalOperation op;
    ac_int<32, true> res;
    ac_int<5, false> rd;
};

class Simulator;

void doStep(ac_int<32, false> startpc, bool &exit,
            unsigned int im[DRAM_SIZE], unsigned int dm[DRAM_SIZE],
            unsigned int cim0[Sets][Blocksize][Associativity], unsigned int cdm0[Sets][Blocksize][Associativity],
            ac_int<IWidth, false> memictrl0[Sets], ac_int<DWidth, false> memdctrl0[Sets],
            MultiCycleOp& mcop0, MultiCycleRes mcres0,
            unsigned int cim1[Sets][Blocksize][Associativity], unsigned int cdm1[Sets][Blocksize][Associativity],
            ac_int<IWidth, false> memictrl1[Sets], ac_int<DWidth, false> memdctrl1[Sets],
            MultiCycleOp& mcop1, MultiCycleRes mcres1
        #ifndef __HLS__
            , Simulator* syscall
        #endif
            );


#endif  // CORE_H
