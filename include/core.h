#ifndef CORE_H
#define CORE_H

#include "portability.h"
#include "riscvISA.h"
#include "cache.h"
#include "multicycleoperator.h"


/******************************************************************************************
 * Definition of all pipeline registers
 *
 * ****************************************************************************************
 */

struct FtoDC
{
    FtoDC() : pc(0), instruction(0x13), we(1), stall(0)
    {}
    ac_int<32, false> pc;           	// PC where to fetch
    ac_int<32, false> instruction;  	// Instruction to execute
    ac_int<32, false> nextPCFetch;      // Next pc according to fetch

    //Register for all stages
    ac_int<1, false> we;
    ac_int<1, false> stall;
};

struct DCtoEx
{
    ac_int<32, false> pc;       // used for branch
    ac_int<32, false> instruction;

    ac_int<7, false> opCode;    // opCode = instruction[6:0]
    ac_int<7, false> funct7;    // funct7 = instruction[31:25]
    ac_int<3, false> funct3;    // funct3 = instruction[14:12]

    ac_int<32, true> lhs;   //  left hand side : operand 1
    ac_int<32, true> rhs;   // right hand side : operand 2
    ac_int<32, true> datac; // ST, BR, JAL/R,

    // syscall only
    ac_int<32, true> datad;
    ac_int<32, true> datae;

    //For branch unit
    ac_int<32, false> nextPCDC;
    ac_int<1, false> isBranch;

    //Information for forward/stall unit
    ac_int<1, false> useRs1;
    ac_int<1, false> useRs2;
    ac_int<1, false> useRd;
    ac_int<5, false> rs1;       // rs1    = instruction[19:15]
    ac_int<5, false> rs2;       // rs2    = instruction[24:20]
    ac_int<5, false> rd;        // rd     = instruction[11:7]

    //Register for all stages
    ac_int<1, false> we;
    ac_int<1, false> stall; //TODO add that
};

struct ExtoMem
{
    ac_int<32, false> pc;
    ac_int<32, false> instruction;

    ac_int<32, true> result;    // result of the EX stage
    ac_int<5, false> rd;        // destination register
    ac_int<7, false> opCode;    // LD or ST (can be reduced to 2 bits)
    ac_int<3, false> funct3;    // datasize and sign extension bit

    ac_int<32, true> datac;     // data to be stored in memory or csr result

    //For branch unit
    ac_int<32, false> nextPC;
    ac_int<1, false> isBranch;

    //Register for all stages
    ac_int<1, false> we;
    ac_int<1, false> stall; //TODO add that
};

struct MemtoWB
{
    ac_int<32, true> result;    // Result to be written back
    ac_int<5, false> rd;        // destination register

    //Register for all stages
    ac_int<1, false> we;
    ac_int<1, false> stall;

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

    /// Multicycle operation
    MultiCycleOperator mcop;
    MultiCycleRes mcres;

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
            MultiCycleOperator& mcop, MultiCycleRes mcres,
            unsigned int im[DRAM_SIZE], unsigned int dm[DRAM_SIZE],
            unsigned int cim[Sets][Blocksize][Associativity], unsigned int cdm[Sets][Blocksize][Associativity],
            ac_int<IWidth, false> memictrl[Sets], ac_int<DWidth, false> memdctrl[Sets]
        #ifndef __HLS__
            , Simulator* syscall
        #endif
            );


#endif  // CORE_H
