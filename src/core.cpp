#include "core.h"

#ifndef __HLS__
#include "simulator.h"

#define EXDEFAULT() default: \
    dbgassert(false, "Error : Unknown operation in Ex stage : @%06x	%08x\n", core.extoMem.pc.to_int(), core.extoMem.instruction.to_int()); \
    break;

#else

#define EXDEFAULT()
#define fprintf(...)

#endif  // __HLS__

#ifndef COMET_NO_CSR
//template<unsigned int hartid>
const ac_int<32, false> CSR::mvendorid = 0;
//template<unsigned int hartid>
const ac_int<32, false> CSR::marchid = 0;
//template<unsigned int hartid>
const ac_int<32, false> CSR::mimpid = 0;
#endif

void Fetch(ac_int<32, false> pc,
			struct FtoDC &ftoDC,
			ac_int<32, false> instructionMemory[])
{
    ftoDC.instruction = instructionMemory[pc/4];
    ftoDC.pc = pc;
    ftoDC.nextPCFetch = pc + 4;

    ftoDC.stall = 0;
    ftoDC.we = 1;
}

void DC(struct FtoDC ftoDC,
		struct DCtoEx &dctoEx,
		ac_int<32, false> registerFile)
{
    ac_int<32, false> pc = ftoDC.pc;
    ac_int<32, false> instruction = ftoDC.instruction;

    // R-type instruction
    ac_int<7, false> funct7 = instruction.slc<7>(25);
    ac_int<5, false> rs2 = instruction.slc<5>(20);
    ac_int<5, false> rs1 = instruction.slc<5>(15);
    ac_int<3, false> funct3 = instruction.slc<3>(12);
    ac_int<5, false> rd = instruction.slc<5>(7);
    ac_int<7, false> opCode = instruction.slc<7>(0);    // could be reduced to 5 bits because 1:0 is always 11

    //Construction of different immediate values
    ac_int<12, false> imm12_I = instruction.slc<12>(20);
	ac_int<12, false> imm12_S = 0;
	imm12_S.set_slc(5, instruction.slc<7>(25));
	imm12_S.set_slc(0, instruction.slc<5>(7));

	ac_int<12, true> imm12_I_signed = instruction.slc<12>(20);
	ac_int<12, true> imm12_S_signed = 0;
	imm12_S_signed.set_slc(0, imm12_S.slc<12>(0));

	ac_int<13, false> imm13 = 0;
	imm13[12] = instruction[31];
	imm13.set_slc(5, instruction.slc<6>(25));
	imm13.set_slc(1, instruction.slc<4>(8));
	imm13[11] = instruction[7];

	ac_int<13, true> imm13_signed = 0;
	imm13_signed.set_slc(0, imm13);

	ac_int<32, true> imm31_12 = 0;
	imm31_12.set_slc(12, instruction.slc<20>(12));

	ac_int<21, false> imm21_1 = 0;
	imm21_1.set_slc(12, instruction.slc<8>(12));
	imm21_1[11] = instruction[20];
	imm21_1.set_slc(1, instruction.slc<10>(21));
	imm21_1[20] = instruction[31];

	ac_int<21, true> imm21_1_signed = 0;
	imm21_1_signed.set_slc(0, imm21_1);


	//Register access
	ac_int<32, false> valueReg1 = registerFile[rs1];
	ac_int<32, false> valueReg2 = registerFile[rs2];


	dctoEx.rs1 = rs1;
	dctoEx.rs2 = rs2;
	dctoEx.rd = rd;
	dctoEx.opCode = opCode;
	dctoEx.funct3 = funct3;
	dctoEx.funct7 = funct7;
	dctoEx.instruction = instruction;
	dctoEx.pc = pc;

	//Initialization of control bits
	dctoEx.useRs1 = 0;
	dctoEx.useRs2 = 0;
	dctoEx.useRd = 0;
	dctoEx.we = ftoDC.we;
	dctoEx.isBranch = 0;



	switch (opCode)
	{
	case RISCV_LUI:
		dctoEx.lhs = imm31_12;
		dctoEx.useRs1 = 0;
		dctoEx.useRs2 = 0;
		dctoEx.useRd = 1;

	break;
	case RISCV_AUIPC:
		dctoEx.lhs = ftoDC.pc;
		dctoEx.rhs = imm31_12;
		dctoEx.useRs1 = 0;
		dctoEx.useRs2 = 0;
		dctoEx.useRd = 1;
	break;
	case RISCV_JAL:
		dctoEx.lhs = ftoDC.pc+4;
		dctoEx.rhs = 0;
		dctoEx.nextPCDC = ftoDC.pc + imm21_1_signed;
		dctoEx.useRs1 = 0;
		dctoEx.useRs2 = 0;
		dctoEx.useRd = 1;
		dctoEx.isBranch = 1;

	break;
	case RISCV_JALR:
		dctoEx.lhs = ftoDC.pc+4;
		dctoEx.rhs = 0;
		dctoEx.useRs1 = 0;
		dctoEx.useRs2 = 0;
		dctoEx.useRd = 1;

		dctoEx.nextPCDC = ftoDC.pc + imm21_1_signed;
		dctoEx.isBranch = 1;

	break;
	case RISCV_BR:

		dctoEx.lhs = valueReg1;
		dctoEx.rhs = valueReg2;
		dctoEx.useRs1 = 1;
		dctoEx.useRs2 = 1;
		dctoEx.useRd = 0;

	break;
	case RISCV_LD:

		dctoEx.lhs = valueReg1;
		dctoEx.rhs = imm12_I_signed;
		dctoEx.useRs1 = 1;
		dctoEx.useRs2 = 0;
		dctoEx.useRd = 1;

	break;

	//******************************************************************************************
	//Treatment for: STORE INSTRUCTIONS
	case RISCV_ST:
		dctoEx.lhs = valueReg1;
		dctoEx.rhs = imm12_S_signed;
		dctoEx.datac = valueReg2; //Value to store in memory
		dctoEx.useRs1 = 1;
		dctoEx.useRs2 = 1;
		dctoEx.useRd = 0;
	break;
	case RISCV_OPI:
		dctoEx.lhs = valueReg1;
		dctoEx.rhs = imm12_I_signed;
		dctoEx.useRs1 = 1;
		dctoEx.useRs2 = 0;
		dctoEx.useRd = 1;
	break;

	case RISCV_OP:

		dctoEx.lhs = valueReg1;
		dctoEx.rhs = valueReg2;
		dctoEx.useRs1 = 1;
		dctoEx.useRs2 = 1;
		dctoEx.useRd = 1;

	break;
	case RISCV_SYSTEM:

		//TODO

	break;
	default:

	break;

	}

}

void Execute(struct DCtoEx dctoEx,
			struct ExtoMem &extoMem)
{

    extoMem.pc = dctoEx.pc;
    extoMem.opCode = dctoEx.opCode;
    extoMem.rd = dctoEx.rd;
    extoMem.funct3 = dctoEx.funct3;
    extoMem.we = dctoEx.we;
    extoMem.isBranch = 0;

	ac_int<13, false> imm13 = 0;
	imm13[12] = dctoEx.instruction[31];
	imm13.set_slc(5, dctoEx.instruction.slc<6>(25));
	imm13.set_slc(1, dctoEx.instruction.slc<4>(8));
	imm13[11] = dctoEx.instruction[7];

	ac_int<13, true> imm13_signed = 0;
	imm13_signed.set_slc(0, imm13);

	ac_int<5, false> shamt = dctoEx.instruction.slc<5>(20);


	// switch must be in the else, otherwise external op may trigger default case
	switch(dctoEx.opCode)
	{
	case RISCV_LUI:
		extoMem.result = dctoEx.lhs;
		break;
	case RISCV_AUIPC:
		extoMem.result = dctoEx.lhs + dctoEx.rhs;
		break;
	case RISCV_JAL:
		//Note: in current version, the addition is made in the decode stage
		//The value to store in rd (pc+4) is stored in lhs
		extoMem.result = dctoEx.lhs;
		break;
	case RISCV_JALR:
		//Note: in current version, the addition is made in the decode stage
		//The value to store in rd (pc+4) is stored in lhs
		extoMem.result = dctoEx.lhs;
		break;
	case RISCV_BR:
		switch(dctoEx.funct3)
		{
		extoMem.nextPC = extoMem.pc + imm13_signed;

		case RISCV_BR_BEQ:
			extoMem.isBranch = (dctoEx.lhs == dctoEx.rhs);
			break;
		case RISCV_BR_BNE:
			extoMem.isBranch = (dctoEx.lhs != dctoEx.rhs);
			break;
		case RISCV_BR_BLT:
			extoMem.isBranch = (dctoEx.lhs < dctoEx.rhs);
			break;
		case RISCV_BR_BGE:
			extoMem.isBranch = (dctoEx.lhs >= dctoEx.rhs);
			break;
		case RISCV_BR_BLTU:
			extoMem.isBranch = ((ac_int<32, false>)dctoEx.lhs < (ac_int<32, false>)dctoEx.rhs);
			break;
		case RISCV_BR_BGEU:
			extoMem.isBranch = ((ac_int<32, false>)dctoEx.lhs >= (ac_int<32, false>)dctoEx.rhs);
			break;
		}
		break;
	case RISCV_LD:
		extoMem.result = dctoEx.lhs + dctoEx.rhs;
		break;
	case RISCV_ST:
		extoMem.result = dctoEx.lhs + dctoEx.rhs;
		break;
	case RISCV_OPI:
		switch(dctoEx.funct3)
		{
		case RISCV_OPI_ADDI:
			extoMem.result = dctoEx.lhs + dctoEx.rhs;
			break;
		case RISCV_OPI_SLTI:
			extoMem.result = dctoEx.lhs < dctoEx.rhs;
			break;
		case RISCV_OPI_SLTIU:
			extoMem.result = (ac_int<32, false>)dctoEx.lhs < (ac_int<32, false>)dctoEx.rhs;
			break;
		case RISCV_OPI_XORI:
			extoMem.result = dctoEx.lhs ^ dctoEx.rhs;
			break;
		case RISCV_OPI_ORI:
			extoMem.result = dctoEx.lhs | dctoEx.rhs;
			break;
		case RISCV_OPI_ANDI:
			extoMem.result = dctoEx.lhs & dctoEx.rhs;
			break;
		case RISCV_OPI_SLLI: // cast rhs as 5 bits, otherwise generated hardware is 32 bits
			// & shift amount held in the lower 5 bits (riscv spec)
			extoMem.result = dctoEx.lhs << (ac_int<5, false>)dctoEx.rhs;
			break;
		case RISCV_OPI_SRI:
			if (dctoEx.funct7.slc<1>(5)) //SRAI
				extoMem.result = dctoEx.lhs >> (ac_int<5, false>)shamt;
			else //SRLI
				extoMem.result = (ac_int<32, false>)dctoEx.lhs >> (ac_int<5, false>)shamt;
			break;
		}
		break;
	case RISCV_OP:
		if(dctoEx.funct7.slc<1>(0))     // M Extension
		{
			ac_int<33, true> mul_reg_a = dctoEx.lhs;
			ac_int<33, true> mul_reg_b = dctoEx.rhs;
			ac_int<66, true> longResult = 0;
			switch (dctoEx.funct3)  //Switch case for multiplication operations (RV32M)
			{
			case RISCV_OP_M_MULHSU:
				mul_reg_b[32] = 0;
				break;
			case RISCV_OP_M_MULHU:
				mul_reg_a[32] = 0;
				mul_reg_b[32] = 0;
				break;
			}
			longResult = mul_reg_a * mul_reg_b;
			if(dctoEx.funct3 == RISCV_OP_M_MULH || dctoEx.funct3 == RISCV_OP_M_MULHSU || dctoEx.funct3 == RISCV_OP_M_MULHU)
				extoMem.result = longResult.slc<32>(32);
			else
				extoMem.result = longResult.slc<32>(0);
		}
		else{
			switch(dctoEx.funct3){
			case RISCV_OP_ADD:
				if (dctoEx.funct7.slc<1>(5))   // SUB
					extoMem.result = dctoEx.lhs - dctoEx.rhs;
				else   // ADD
					extoMem.result = dctoEx.lhs + dctoEx.rhs;
				break;
			case RISCV_OP_SLL:
				extoMem.result = dctoEx.lhs << (ac_int<5, false>)dctoEx.rhs;
				break;
			case RISCV_OP_SLT:
				extoMem.result = dctoEx.lhs < dctoEx.rhs;
				break;
			case RISCV_OP_SLTU:
				extoMem.result = (ac_int<32, false>)dctoEx.lhs < (ac_int<32, false>)dctoEx.rhs;
				break;
			case RISCV_OP_XOR:
				extoMem.result = dctoEx.lhs ^ dctoEx.rhs;
				break;
			case RISCV_OP_SR:
				if(dctoEx.funct7.slc<1>(5))   // SRA
					extoMem.result = dctoEx.lhs >> (ac_int<5, false>)dctoEx.rhs;
				else  // SRL
					extoMem.result = (ac_int<32, false>)dctoEx.lhs >> (ac_int<5, false>)dctoEx.rhs;
				break;
			case RISCV_OP_OR:
				extoMem.result = dctoEx.lhs | dctoEx.rhs;
				break;
			case RISCV_OP_AND:
				extoMem.result = dctoEx.lhs & dctoEx.rhs;
				break;
			}
		}
		break;
	case RISCV_MISC_MEM:    // this does nothing because all memory accesses are ordered and we have only one core
		break;

	case RISCV_SYSTEM:
		switch(dctoEx.funct3)
		{ // case 0: mret instruction, dctoEx.memValue should be 0x302
		case RISCV_SYSTEM_ENV:
			#ifndef __HLS__
			extoMem.result = sim->solveSyscall(dctoEx.lhs, dctoEx.rhs, dctoEx.datac, dctoEx.datad, dctoEx.datae, exit);
			#endif
			break;
		case RISCV_SYSTEM_CSRRW:    // lhs is from csr, rhs is from reg[rs1]
			extoMem.datac = dctoEx.rhs;       // written back to csr
			extoMem.result = dctoEx.lhs;      // written back to rd
			break;
		case RISCV_SYSTEM_CSRRS:
			extoMem.datac = dctoEx.lhs | dctoEx.rhs;
			extoMem.result = dctoEx.lhs;
			break;
		case RISCV_SYSTEM_CSRRC:
			extoMem.datac = dctoEx.lhs & ((ac_int<32, false>)~dctoEx.rhs);
			extoMem.result = dctoEx.lhs;
			break;
		case RISCV_SYSTEM_CSRRWI:
			extoMem.datac = dctoEx.rhs;
			extoMem.result = dctoEx.lhs;
			break;
		case RISCV_SYSTEM_CSRRSI:
			extoMem.datac = dctoEx.lhs | dctoEx.rhs;
			extoMem.result = dctoEx.lhs;
			break;
		case RISCV_SYSTEM_CSRRCI:
			extoMem.datac = dctoEx.lhs & ((ac_int<32, false>)~dctoEx.rhs);
			extoMem.result = dctoEx.lhs;
			break;
		}
		break;
	}
}

void Memory(struct ExtoMem extoMem,
                        struct MemToWB &memtoWB,
            ac_int<32, true> data_memory[DRAM_SIZE])
{

    ac_int<2, false> datasize = extoMem.funct3.slc<2>(0);
    ac_int<1, false> signenable = !extoMem.funct3.slc<1>(2);
    memtoWB.we = extoMem.we;
        switch(extoMem.opCode)
        {
        case RISCV_LD:
            memtoWB.rd = extoMem.rd;
            ac_int<32, false> mem_read = data_memory[extoMem.result >> 2];
            formatread(extoMem.result, datasize, signenable, mem_read);
            memtoWB.result = mem_read;
            break;
        case RISCV_ST:
            memtoWB.rd = 0;
            ac_int<32, false> memory_val = data_memory[extoMem.result >> 2];
            if(extoMem.we)
                formatwrite(extoMem.result, datasize, memory_val, extoMem.datac);
            address = extoMem.result;
            writevalue = memory_val;   // dctrl is not used anyway
            break;
        default:
            memtoWB.result = extoMem.result;
            memtoWB.rd = extoMem.rd;
            break;
        }
    }
}

void Writeback(struct MemToWB memtoWB,
               ac_int<32, false> registerFile)
{
    if((memtoWB.rd != 0) && (memtoWB.we))
        registerFile[memtoWB.rd] = memtoWB.result;
}

void coreinit(Core& core, ac_int<32, false> startpc)
{
    core.ctrl.init = true;

    core.pc = startpc;
    if(startpc)
        core.ctrl.sleep = false;
    else
        core.ctrl.sleep = true;
}

template<unsigned int hartid>
void doCore(ac_int<32, false> startpc, bool &exit,
            MultiCycleOperator& mcop, MultiCycleRes mcres,
        #ifdef nocache
            unsigned int im[DRAM_SIZE], unsigned int dm[DRAM_SIZE]
        #else
            ICacheRequest& ireq, ICacheReply irep,
            DCacheRequest& dreq, DCacheReply drep
        #endif
        #ifndef __HLS__
            , Simulator* sim
        #endif
            )
{
    static Core core;

    if(!core.ctrl.init)
    {
        coreinit(core, startpc);

        simul(sim->setCore(&core);)
    }

    /// Unconditionnal read
    core.mcres = mcres;
#ifdef nocache
    simul(uint64_t oldcycles = core.csrs.mcycle;)
#else
    core.ireply = irep;
    core.dreply = drep;
#endif

    //if(!core.ctrl.sleep)
    {
        doWB(core);
        simul(coredebug("%lld ", core.csrs.mcycle.to_int64());
        for(int i=0; i<32; i++)
        {
            if(core.REG[i])
                coredebug("%d:%08x ", i, (int)core.REG[i]);
        }
        coredebug("\n");)

        do_Mem(core
        #ifdef nocache
               , dm
           #ifndef __HLS__
               , core.csrs.mcycle
           #endif
        #endif
               );

        if(!core.ctrl.cachelock)
        {
            Ex(core
        #ifndef __HLS__
               , exit, sim
        #endif
               );
            DC<hartid>(core
        #ifdef __HLS__
                       , exit
        #endif
              );
            Ft(core
       #ifdef nocache
           , im
           #ifndef __HLS__
               , core.csrs.mcycle
           #endif
       #endif
              );

            if(core.ctrl.prev_opCode[2] == RISCV_LD || core.mcres.done)
                core.ctrl.prev_res[2] = core.memtoWB.result;    // store the loaded value, not the address
                // this works because we know that we can't have lw a5, x   addi a0, a5, 1
                // there's always a nop between load and next use of value (& we insert bubble for external op)
            else
                core.ctrl.prev_res[2] = core.ctrl.prev_res[1];
            core.ctrl.prev_res[1] = core.ctrl.prev_res[0];

            // if we had a branch, zeros everything
            if(core.ctrl.branch[1])
            {
                core.ctrl.prev_opCode[2] = core.ctrl.prev_opCode[1] = RISCV_OPI;
                core.ctrl.prev_rds[2] = core.ctrl.prev_rds[1] = 0;  // prevent useless dependencies
                core.ctrl.branch[2] = core.ctrl.branch[1];
                core.ctrl.branch[1] = core.ctrl.branch[0] = 0;      // prevent taking wrong branch
            }
            else
            {
                core.ctrl.prev_opCode[2] = core.ctrl.prev_opCode[1];
                core.ctrl.prev_opCode[1] = core.ctrl.prev_opCode[0];
                core.ctrl.prev_rds[2] = core.ctrl.prev_rds[1];
                core.ctrl.prev_rds[1] = core.ctrl.prev_rds[0];
                core.ctrl.branch[2] = core.ctrl.branch[1];
                core.ctrl.branch[1] = core.ctrl.branch[0];
            }
            core.ctrl.jump_pc[1] = core.ctrl.jump_pc[0];
        }
        
        core.csrs.mcycle += 1;

    #ifdef nocache
        simul(
        int M = MEMORY_READ_LATENCY>MEMORY_WRITE_LATENCY?MEMORY_READ_LATENCY:MEMORY_WRITE_LATENCY;
        if(oldcycles + M < core.csrs.mcycle)
        {
            core.csrs.mcycle = oldcycles + M; // we cannot step slower than the worst latency
        }
        )
    #endif

        // riscv specification v2.2 p11
        dbgassert((core.pc.to_int() & 3) == 0, "Misaligned instruction @%06x\n",
                   core.pc.to_int());
    }

    /// Unconditionnal write
    mcop = core.mcop;
#ifndef nocache
    ireq = core.irequest;
    dreq = core.drequest;
#endif

}

void doStep(ac_int<32, false> startpc, bool &exit,
            MultiCycleOperator& mcop, MultiCycleRes mcres,
            unsigned int im[DRAM_SIZE], unsigned int dm[DRAM_SIZE],
            unsigned int cim[Sets][Blocksize][Associativity], unsigned int cdm[Sets][Blocksize][Associativity],
            ac_int<IWidth, false> memictrl[Sets], ac_int<DWidth, false> memdctrl[Sets]
        #ifndef __HLS__
            , Simulator* sim
        #endif
            )
{
    static ICacheRequest ireq; static ICacheReply irep;
    static DCacheRequest dreq; static DCacheReply drep;

    doCore<0>(startpc, exit,
              mcop, mcres,
          #ifdef nocache
              im, dm
          #else
              ireq, irep,
              dreq, drep
          #endif
          #ifndef __HLS__
              , sim
          #endif
              );

#ifndef nocache
    // cache should maybe be all the way up or down
    // cache down generates less hardware and is less cycle costly(20%?)
    // but cache up has a slightly better critical path
    dcache(memdctrl, dm, cdm, dreq, drep
       #ifndef __HLS__
           , sim
       #endif
           );
    icache(memictrl, im, cim, ireq, irep
       #ifndef __HLS__
           , sim
       #endif
           );
#endif

    // doCore<1>(...)
    // directory cache control
}
