#ifndef CORE_H
#define CORE_H

#include "portability.h"
#include "simulator.h"

struct FtoDC
{
    ac_int<32, false> pc;
    ac_int<32, false> instruction; //Instruction to execute
};

struct DCtoEx
{
    ac_int<32, false> pc;
    ac_int<32, false> instruction;
    ac_int<32, true> dataa; //First data from register file
    ac_int<32, true> datab; //Second data, from register file or immediate value
    ac_int<32, true> datac;
    ac_int<32, true> datad; //Third data used only for store instruction and corresponding to rb
    ac_int<32, true> datae;
    ac_int<5, false> dest; //Register to be written
    ac_int<7, false> opCode;//OpCode of the instruction
    ac_int<32, true> memValue; //Second data, from register file or immediate value
    ac_int<7, false> funct3;
    ac_int<7, false> funct7;
    ac_int<5, false> shamt;
    ac_int<5, false> rs1;
    ac_int<5, false> rs2;
};

struct ExtoMem
{
    ac_int<32, false> pc;
    ac_int<32, false> instruction;
    ac_int<32, true> result; //Result of the EX stage
    ac_int<32, true> datad;
    ac_int<32, true> datac; //Data to be stored in memory (if needed)
    ac_int<5, false> dest; //Register to be written at WB stage
    bool WBena; //Is a WB is needed ?
    ac_int<7, false> opCode; //OpCode of the operation
    ac_int<32, true> memValue; //Second data, from register file or immediate value
    ac_int<5, false> rs1;
    ac_int<7, false> funct3;
    ac_int<2, false> sys_status;
};

struct MemtoWB
{
    ac_int<32, false> lastpc;
    ac_int<32, false> pc;
    ac_int<32, false> instruction;
    ac_int<32, true> result; //Result to be written back
    ac_int<32, false> rescsr;   // Result for CSR instruction
    ac_int<12, false> CSRid;    // CSR to be written back
    ac_int<5, false> dest; //Register to be written at WB stage
    bool WBena; //Is a WB is needed ?
    ac_int<7, false> opCode;
    ac_int<2, false> sys_status;
    ac_int<5, false> rs1;
    bool csrwb;
};

struct CSR
{
    static const ac_int<32, false> mvendorid;   // RO shared by all cores
    static const ac_int<32, false> marchid;     // RO shared by all cores
    static const ac_int<32, false> mimpid;      // RO shared by all cores
    const ac_int<32, false> mhartid;            // RO but private to core (and i don't want to template everything)
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

struct Core
{
    FtoDC ftoDC;
    DCtoEx dctoEx;
    ExtoMem extoMem;
    MemtoWB memtoWB;
    CSR csrs;

    ac_int<32, true> REG[32];
    ac_int<7, false> prev_opCode;
    ac_int<32, false> prev_pc;

    ac_int<3, false> mem_lock;
    bool early_exit;

    bool freeze_fetch;
    bool ex_bubble;
    bool mem_bubble;
    bool cachelock;
    ac_int<32, false> pc;
    bool init;
};

void doStep(ac_int<32, false> pc, unsigned int ins_memory[N], unsigned int dm[N], bool &exit
        #ifndef __SYNTHESIS__
            , ac_int<64, false>& c, ac_int<64, false>& numins, Simulator* syscall
        #endif
            );


#endif  // CORE_H
