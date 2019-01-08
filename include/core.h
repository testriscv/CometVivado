#ifndef CORE_H
#define CORE_H

#include <ac_int.h>
#include "riscvISA.h"


#define INSTR_MEMORY_WIDTH 32
#define DATA_MEMORY_WIDTH
#define DRAM_SIZE 8192

/******************************************************************************************
 * Definition of all pipeline registers
 *
 * ****************************************************************************************
 */

struct ForwardReg {
	ac_int<1, false> forwardWBtoVal1;
	ac_int<1, false> forwardWBtoVal2;
	ac_int<1, false> forwardWBtoVal3;

	ac_int<1, false> forwardMemtoVal1;
	ac_int<1, false> forwardMemtoVal2;
	ac_int<1, false> forwardMemtoVal3;

	ac_int<1, false> forwardExtoVal1;
	ac_int<1, false> forwardExtoVal2;
	ac_int<1, false> forwardExtoVal3;
};

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
    ac_int<1, false> useRs3;
    ac_int<1, false> useRd;
    ac_int<5, false> rs1;       // rs1    = instruction[19:15]
    ac_int<5, false> rs2;       // rs2    = instruction[24:20]
    ac_int<5, false> rs3;
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
    ac_int<1, false> useRd;
    ac_int<1, false> isLongInstruction;
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
    ac_int<1, false> useRd;

    ac_int<32, true> address;
    ac_int<32, true> valueToWrite;
    ac_int<4, false> byteEnable;
    ac_int<1, true> isStore;

    //Register for all stages
    ac_int<1, false> we;
    ac_int<1, false> stall;

};

struct WBOut
{
	ac_int<32, false> value;
	ac_int<5, false> rd;
	ac_int<1, false> useRd;
    ac_int<1, false> we;
};

struct Core
{
    FtoDC ftoDC;
    DCtoEx dctoEx;
    ExtoMem extoMem;
    MemtoWB memtoWB;

    //CoreCtrl ctrl;

    ac_int<32, true> REG[32];
    ac_int<32, false> pc;

    /// Multicycle operation

    /// Instruction cache
    //unsigned int idata[Sets][Blocksize][Associativity];   // made external for modelsim


    /// Data cache
    //unsigned int ddata[Sets][Blocksize][Associativity];   // made external for modelsim

};

//Functions for copying values
void copyFtoDC(struct FtoDC &dest, struct FtoDC src);
void copyDCtoEx(struct DCtoEx &dest, struct DCtoEx src);
void copyExtoMem(struct ExtoMem &dest, struct ExtoMem src);
void copyMemtoWB(struct MemtoWB &dest, struct MemtoWB src);



class Simulator;



#endif  // CORE_H
