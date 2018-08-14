#include "portability.h"
#include "riscvISA.h"
#include "core.h"
#include "cache.h"
#ifndef __HLS__
#include "simulator.h"

#define EXDEFAULT() default: \
    dbgassert(false, "Error : Unknown operation in Ex stage : @%06x	%08x\n", core.extoMem.pc.to_int(), core.extoMem.instruction.to_int()); \
    break;

#else

#define EXDEFAULT()
#define fprintf(...)

#endif

//template<unsigned int hartid>
const ac_int<32, false> CSR::mvendorid = 0;
//template<unsigned int hartid>
const ac_int<32, false> CSR::marchid = 0;
//template<unsigned int hartid>
const ac_int<32, false> CSR::mimpid = 0;

void Ft(Core& core, unsigned int im[DRAM_SIZE]
    #ifndef __HLS__
        , ac_int<64, false>& cycles
    #endif
        )
{
    ac_int<32, false> next_pc = core.pc + 4;
#ifndef nocache
    if(core.ctrl.freeze_fetch)
    {
        // LD dependency, do nothing
        gdebug("Fetch frozen\n");
    }
    else
    {
        if(core.insvalid && core.cachepc == core.pc)
        {
            core.ftoDC.instruction = core.instruction;
            core.ftoDC.pc = core.pc;
            core.ftoDC.realInstruction = true;

            switch(core.ctrl.prev_opCode[2])
            {
            case RISCV_BR:  // check if branch was taken
                if(core.ctrl.branch[1])
                    core.pc = core.ctrl.jump_pc[1];
                else
                    core.pc = next_pc;
                break;
            case RISCV_JAL:
                core.pc = core.ctrl.jump_pc[1];
                break;
            case RISCV_JALR:
                core.pc = core.ctrl.jump_pc[1];
                break;
            default:
                core.pc = next_pc;
                break;
            }
        }
        else
        {
            core.ftoDC.instruction = 0x13;  // insert bubble (0x13 is NOP instruction and corresponds to ADDI r0, r0, 0)
            core.ftoDC.pc = 0;
            core.ftoDC.realInstruction = false;

            // update pc for branch/jump when miss
            switch(core.ctrl.prev_opCode[2])
            {
            case RISCV_BR:  // check if branch was taken
                if(core.ctrl.branch[1])
                    core.pc = core.ctrl.jump_pc[1];
                else
                    core.pc = core.pc;
                break;
            case RISCV_JAL:
                core.pc = core.ctrl.jump_pc[1];
                break;
            case RISCV_JALR:
                core.pc = core.ctrl.jump_pc[1];
                break;
            default:
                core.pc = core.pc;
                break;
            }
        }
        core.ftoDC.nextpc = next_pc;
    }

    core.iaddress = core.pc;
    gdebug("%06x\n", core.iaddress.to_int());

    simul(if(core.ftoDC.realInstruction)
    {
        coredebug("Ft   @%06x   %08x\n", core.ftoDC.pc.to_int(), core.ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
#else
    if(!core.ctrl.freeze_fetch)
    {
        core.ftoDC.instruction = im[core.pc/4];
        simul(cycles += MEMORY_READ_LATENCY);
        core.ftoDC.pc = core.pc;
        core.ftoDC.realInstruction = true;
        core.ftoDC.nextpc = next_pc;

        switch(core.ctrl.prev_opCode[2])
        {
        case RISCV_BR:  // check if branch was taken
            if(core.ctrl.branch[1])
                core.pc = core.ctrl.jump_pc[1];
            else
                core.pc = next_pc;
            break;
        case RISCV_JAL:
            core.pc = core.ctrl.jump_pc[1];
            break;
        case RISCV_JALR:
            core.pc = core.ctrl.jump_pc[1];
            break;
        default:
            core.pc = next_pc;
            break;
        }
    }
    else      // we cannot overwrite because it would not execute the frozen instruction
    {
        core.ftoDC.instruction = core.ftoDC.instruction;
        core.ftoDC.pc = core.ftoDC.pc;
        core.ftoDC.realInstruction = true;

        switch(core.ctrl.prev_opCode[2])
        {
        case RISCV_BR:  // check if branch was taken
            if(core.ctrl.branch[1])
                core.pc = core.ctrl.jump_pc[1];
            else
                core.pc = core.pc;
            break;
        case RISCV_JAL:
            core.pc = core.ctrl.jump_pc[1];
            break;
        case RISCV_JALR:
            core.pc = core.ctrl.jump_pc[1];
            break;
        default:
            core.pc = core.pc;
            break;
        }

        gdebug("Fetch frozen\n");
    }

    simul(if(core.ftoDC.pc)
    {
        coredebug("Ft   @%06x   %08x\n", core.ftoDC.pc.to_int(), core.ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
    gdebug("i @%06x   %08x\n", core.pc.to_int(), im[core.pc/4]);
#endif
}

template<unsigned int hartid>
void DC(Core& core
    #ifdef __HLS__
        , bool& exit
    #endif
        )
{
    ac_int<32, false> pc = core.ftoDC.pc;
    ac_int<32, false> instruction = core.ftoDC.instruction;

    // R-type instruction
    ac_int<7, false> funct7 = instruction.slc<7>(25);
    ac_int<5, false> rs2 = instruction.slc<5>(20);
    ac_int<5, false> rs1 = instruction.slc<5>(15);
    ac_int<3, false> funct3 = instruction.slc<3>(12);
    ac_int<5, false> rd = instruction.slc<5>(7);
    ac_int<7, false> opCode = instruction.slc<7>(0);    // reduced to 5 bits because 1:0 is always 11
    // cannot reduce opCode to 5 bits with modelsim (x propagation...)

    dbgassert(instruction.slc<2>(0) == 3, "Instruction lower bits are not 0b11, illegal instruction @%06x : %08x\n",
              pc.to_int(), instruction.to_int());

    ac_int<32, true> rhs = 0;
    ac_int<32, true> lhs = 0;
    ac_int<32, true> datac = 0;
    bool realInstruction = true;   

    bool forward_rs1 = false;
    bool forward_rs2 = false;
    bool forward_datac = false;
    bool forward_ex_or_mem_rs1 = false;
    bool forward_ex_or_mem_rs2 = false;
    bool forward_ex_or_mem_datac = false;

    // prev_rds[1] is ex, prev_rds[2] is mem
    forward_rs1 = (rs1 != 0) && (core.ctrl.prev_rds[1] == rs1 || core.ctrl.prev_rds[2] == rs1);
    forward_rs2 = (rs2 != 0) && (core.ctrl.prev_rds[1] == rs2 || core.ctrl.prev_rds[2] == rs2);
    forward_ex_or_mem_rs1 = core.ctrl.prev_rds[1] == rs1;
    forward_ex_or_mem_rs2 = core.ctrl.prev_rds[1] == rs2;

    core.ctrl.freeze_fetch = 0;
#ifdef __HLS__
    exit = false;
#endif

    if(core.ctrl.prev_opCode[2] == RISCV_BR && core.ctrl.branch[1])
    {       // a branch was taken
        core.ctrl.lock = 1;
        opCode = RISCV_OPI;
        funct3 = RISCV_OPI_ADDI;
        funct7 = funct7;
        rs1 = 0;
        rs2 = 0;
        rd = 0;
        simul(pc = 0;
        instruction = 0x13;)
        realInstruction = false;
        forward_rs1 = false;
        forward_rs2 = false;

        lhs = 0;
        rhs = 0;
        datac = 0;
    }
    else if(core.ctrl.lock)
    {       // a branch or a jump was taken
        core.ctrl.lock -= 1;
        opCode = RISCV_OPI;
        funct3 = RISCV_OPI_ADDI;
        funct7 = funct7;
        rs1 = 0;
        rs2 = 0;
        rd = 0;
        simul(pc = 0;
        instruction = 0x13;)
        realInstruction = false;
        forward_rs1 = false;
        forward_rs2 = false;

        lhs = 0;
        rhs = 0;
        datac = 0;
    }
    else if(core.ctrl.prev_opCode[1] == RISCV_LD && (core.ctrl.prev_rds[1] == rs1
            || core.ctrl.prev_rds[1] == rs2))
    {
        /// duplicate instruction to remove freeze fetch
        /// and the dependency from dc to ft
        /// but how to handle it?
        /// make freeze_fetch an array as well, and retain next instruction?

        core.ctrl.freeze_fetch = 1;
        opCode = RISCV_OPI;
        funct3 = RISCV_OPI_ADDI;
        funct7 = funct7;
        rs1 = 0;
        rs2 = 0;
        rd = 0;
        simul(pc = 0;
        instruction = 0x13;)
        realInstruction = false;
        forward_rs1 = false;
        forward_rs2 = false;

        lhs = 0;
        rhs = 0;
        datac = 0;
    }
    else        // do normal switch
    {
        // share immediate for all type of instruction
        // this should simplify the hardware (or not apparently, althouh we test less bits)
        // refer to the Table 22.1: RISC-V base opcode map, p133 in Instruction set listings of the spec
        // double cast as signed int before equality for sign extension
        ac_int<32, false> immediate = (ac_int<32, true>)(((ac_int<32, true>)instruction).slc<1>(31));

        lhs = core.REG[rs1];
        rhs = core.REG[rs2];

        realInstruction = core.ftoDC.realInstruction;

        switch(opCode)
        {
        // R-type instruction
        case RISCV_OP:
            break;
        case RISCV_ATOMIC:
            dbgassert(opCode != RISCV_ATOMIC, "Atomic operation not implemented yet @%06x   %08x\n",
                      pc.to_int(), instruction.to_int());
            break;
        // S-type instruction
        // use rs2 rs1 funct3 opCode from R-type
        // 12 bits immediate
        case RISCV_ST:
            immediate.set_slc(5, instruction.slc<6>(25));
            immediate.set_slc(0, instruction.slc<5>(7));

            forward_datac = forward_rs2;
            forward_ex_or_mem_datac = forward_ex_or_mem_rs2;
            datac = rhs;

            rhs = immediate;
            forward_rs2 = false;
            rd = 0;
            break;
        // B-type instruction
        // use rs2 rs1 funct3 opCode from R-type
        // 13 bits immediate
        case RISCV_BR:
            immediate.set_slc(0, (ac_int<1, true>)0);
            immediate.set_slc(1, instruction.slc<4>(8));
            immediate.set_slc(5, instruction.slc<6>(25));
            immediate.set_slc(11, instruction.slc<1>(7));

            datac = immediate;
            rd = 0;
            break;
        // J-type instruction
        // use rd opCode from R-type
        // 20 bits immediate
        case RISCV_JAL:
            immediate.set_slc(0, (ac_int<1, true>)0);
            immediate.set_slc(1, instruction.slc<4>(21));
            immediate.set_slc(5, instruction.slc<6>(25));
            immediate.set_slc(11, instruction.slc<1>(20));
            immediate.set_slc(12, instruction.slc<8>(12));
            lhs = pc;
            rhs = immediate;
            forward_rs1 = false;
            forward_rs2 = false;
            datac = core.ftoDC.nextpc;

            // lock DC for 3 cycles, until we get the real next instruction
            core.ctrl.lock = 3;
            break;
        // U-type instruction
        // use rd opCode from R-type
        // 20 bits immediate
        case RISCV_LUI:
            immediate.set_slc(0, (ac_int<12, true>)0);
            immediate.set_slc(12, instruction.slc<19>(12));
            lhs = immediate;
            forward_rs1 = false;
            forward_rs2 = false;
            break;
        case RISCV_AUIPC:
            immediate.set_slc(0, (ac_int<12, true>)0);
            immediate.set_slc(12, instruction.slc<19>(12));
            lhs = pc;
            rhs = immediate;
            forward_rs1 = false;
            forward_rs2 = false;
            break;
        // I-type instruction
        // use rs1 funct3 rd opCode from R-type
        // 12 bits immediate
        case RISCV_LD:
            immediate.set_slc(0, instruction.slc<11>(20));
            rhs = immediate;
            forward_rs2 = false;

            break;
        case RISCV_OPI:
            immediate.set_slc(0, instruction.slc<11>(20));

            // condition is useless, but catapult generates negative slack with the other one...
            if(funct3 == RISCV_OPI_SLLI || funct3 == RISCV_OPI_SRI)
                rhs = rs2;
            else
                rhs = immediate;

            // no need to test for SLLI or SRAI because only 5 bits are used in EX stage
            // rhs = immediate;
            forward_rs2 = false;

            break;
        case RISCV_JALR:
            immediate.set_slc(0, instruction.slc<11>(20));
            rhs = immediate;
            forward_rs2 = false;
            datac = core.ftoDC.nextpc;

            // lock DC for 3 cycles, until we get the real next instruction
            core.ctrl.lock = 3;
            break;
        case RISCV_MISC_MEM:
            immediate.set_slc(0, instruction.slc<11>(20));
            rhs = immediate;
            forward_rs2 = false;
            break;
        default:
            dbgassert(false, "Error : Unknown operation in DC stage : @%06x	%08x\n", pc.to_int(), instruction.to_int());
            break;

        case RISCV_SYSTEM:
            immediate.set_slc(0, instruction.slc<11>(20));
            if(funct3 == RISCV_SYSTEM_ENV)
            {
            #ifndef __HLS__
                rd = 10;
                rs1 = 17;
                rs2 = 10;

                forward_rs1 = core.ctrl.prev_rds[1] == rs1 || core.ctrl.prev_rds[2] == rs1;
                forward_rs2 = core.ctrl.prev_rds[1] == rs2 || core.ctrl.prev_rds[2] == rs2;
                forward_ex_or_mem_rs1 = core.ctrl.prev_rds[1] == rs1;
                forward_ex_or_mem_rs2 = core.ctrl.prev_rds[1] == rs2;

                lhs = (forward_rs1 && rs1 != 0) ? (forward_ex_or_mem_rs1 ? core.extoMem.result : core.memtoWB.result) : core.REG[rs1];
                rhs = (forward_rs2 && rs2 != 0) ? (forward_ex_or_mem_rs2 ? core.extoMem.result : core.memtoWB.result) : core.REG[rs2];
                realInstruction = true;

                datac = core.ctrl.prev_rds[1] == 11 ? core.extoMem.result : (core.ctrl.prev_rds[2] == 11 ? core.memtoWB.result : core.REG[11]);
                forward_datac = false;
                core.dctoEx.datad = core.ctrl.prev_rds[1] == 12 ? core.extoMem.result : (core.ctrl.prev_rds[2] == 12 ? core.memtoWB.result : core.REG[12]);
                core.dctoEx.datae = core.ctrl.prev_rds[1] == 13 ? core.extoMem.result : (core.ctrl.prev_rds[2] == 13 ? core.memtoWB.result : core.REG[13]);
            #else
                // ignore ecall in synthesis because it makes no sense
                // we should jump at some address specified by a csr
                exit = true;
            #endif
            }
            else simul(if(funct3 != 0x4))
            {
                immediate &= 0x00000FFF;
                if(funct3.slc<1>(2))
                    rhs = rs1;
                // handle the case for rd = 0 for CSRRW
                // handle WIRI/WARL/etc.
                fprintf(stderr, "%03x :     %x  %x  %x  %x\n", (int)immediate, (int)immediate.slc<2>(10), (int)immediate.slc<2>(8),
                        (int)immediate.slc<1>(6), (int)immediate.slc<3>(0));
                if(immediate.slc<2>(8) == 3)
                {
                    if(immediate.slc<2>(10) == 0)
                    {
                        if(immediate.slc<1>(6) == 0)  // 0x30X
                        {
                            switch(immediate.slc<3>(0))
                            {
                            case 0:
                                lhs = core.csrs.mstatus;
                                break;
                            case 1:
                                lhs = core.csrs.misa;
                                break;
                            case 2:
                                lhs = core.csrs.medeleg;
                                break;
                            case 3:
                                lhs = core.csrs.mideleg;
                                break;
                            case 4:
                                lhs = core.csrs.mie;
                                break;
                            case 5:
                                lhs = core.csrs.mtvec;
                                break;
                            case 6:
                                lhs = core.csrs.mcounteren;
                                break;
                            default:
                                dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", immediate.to_int(), pc.to_int());
                                break;
                            }
                        }
                        else                        // 0x34X
                        {
                            switch(immediate.slc<3>(0))
                            {
                            case 0:
                                lhs = core.csrs.mscratch;
                                break;
                            case 1:
                                lhs = core.csrs.mepc;
                                break;
                            case 2:
                                lhs = core.csrs.mcause;
                                break;
                            case 3:
                                lhs = core.csrs.mtval;
                                break;
                            case 4:
                                lhs = core.csrs.mip;
                                break;
                            default:
                                dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", immediate.to_int(), pc.to_int());
                                break;
                            }
                        }
                    }
                    else if(immediate.slc<2>(10) == 2)
                    {
                        ac_int<2, false> foo = 0;
                        foo.set_slc(0, immediate.slc<1>(1));
                        foo.set_slc(1, immediate.slc<1>(7));
                        switch(foo)
                        {
                        case 0:
                            lhs = core.csrs.mcycle.slc<32>(0);
                            break;
                        case 1:
                            lhs = core.csrs.minstret.slc<32>(0);
                            break;
                        case 2:
                            lhs = core.csrs.mcycle.slc<32>(32);
                            break;
                        case 3:
                            lhs = core.csrs.minstret.slc<32>(32);
                            break;
                        }
                    }
                    else if(immediate.slc<2>(10) == 3)
                    {
                        switch(immediate.slc<2>(0))
                        {
                        case 1:
                            lhs = core.csrs.mvendorid;
                            break;
                        case 2:
                            lhs = core.csrs.marchid;
                            break;
                        case 3:
                            lhs = core.csrs.mimpid;
                            break;
                        case 0:
                            lhs = hartid;
                            break;
                        }
                    }
                    else
                    {
                        dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", immediate.to_int(), pc.to_int());
                    }
                }
                else
                {
                    dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", immediate.to_int(), pc.to_int());
                }
                fprintf(stderr, "Reading %08x in CSR @%03x    @%06x\n", rhs.to_int(), immediate.to_int(), pc.to_int());
                //lhs will contain core.REG[rs1]
            }
            simul(else dbgassert(false, "Unknown operation @%06x    %08x\n",
                                        pc.to_int(), instruction.to_int()));
            break;
        }
    }

    core.ctrl.prev_opCode[0] = opCode;
    core.ctrl.prev_rds[0] = rd;

    core.dctoEx.pc = pc;
    simul(core.dctoEx.instruction = instruction;)
    core.dctoEx.opCode = opCode;
    core.dctoEx.funct3 = funct3;
    core.dctoEx.funct7 = funct7;
    core.dctoEx.rd = rd;
    core.dctoEx.realInstruction = realInstruction;

    core.dctoEx.lhs = lhs;
    core.dctoEx.rhs = rhs;
    core.dctoEx.datac = datac;

    core.dctoEx.forward_lhs = forward_rs1;
    core.dctoEx.forward_rhs = forward_rs2;
    core.dctoEx.forward_datac = forward_datac;
    core.dctoEx.forward_mem_lhs = forward_ex_or_mem_rs1;
    core.dctoEx.forward_mem_rhs = forward_ex_or_mem_rs2;
    core.dctoEx.forward_mem_datac = forward_ex_or_mem_datac;


    simul(if(pc)
    {
        coredebug("Dc   @%06x   %08x\n", pc.to_int(), instruction.to_int());
    }
    else
    {
        coredebug("Dc   \n");
    })
}

void Ex(Core& core
    #ifndef __HLS__
        , bool& exit, Simulator* sim
    #endif
        )
{
    core.extoMem.pc = core.dctoEx.pc;
    simul(core.extoMem.instruction = core.dctoEx.instruction;)
    core.extoMem.opCode = core.dctoEx.opCode;
    core.extoMem.rd = core.dctoEx.rd;

    core.extoMem.realInstruction = core.dctoEx.realInstruction;
    core.extoMem.funct3 = core.dctoEx.funct3;
    core.ctrl.branch[0] = false;

    ac_int<32, true> lhs = 0;
    ac_int<32, true> rhs = 0;

    if(core.dctoEx.forward_lhs)
        if(core.dctoEx.forward_mem_lhs)
            lhs = core.ctrl.prev_res[1];
        else
            lhs = core.ctrl.prev_res[2];
    else
        lhs = core.dctoEx.lhs;

    if(core.dctoEx.forward_rhs)
        if(core.dctoEx.forward_mem_rhs)
            rhs = core.ctrl.prev_res[1];
        else
            rhs = core.ctrl.prev_res[2];
    else
        rhs = core.dctoEx.rhs;

    if(core.dctoEx.forward_datac)   // ST instruction only
        if(core.dctoEx.forward_mem_datac)
            core.extoMem.datac = core.ctrl.prev_res[1];
        else
            core.extoMem.datac = core.ctrl.prev_res[2];
    else
        core.extoMem.datac = core.dctoEx.datac;;

    switch (core.dctoEx.opCode)
    {
    case RISCV_LUI:
        core.extoMem.result = lhs;
        break;
    case RISCV_AUIPC:
        core.extoMem.result = lhs + rhs;
        break;
    case RISCV_JAL:
        core.ctrl.jump_pc[0] = lhs + rhs;
        core.extoMem.result = core.dctoEx.datac;   // pc + 4
        break;
    case RISCV_JALR:
        core.ctrl.jump_pc[0] = (lhs + rhs) & 0xfffffffe;   // lsb must be zeroed (cf spec)
        core.extoMem.result = core.dctoEx.datac;   // pc + 4
        break;
    case RISCV_BR:
        switch(core.dctoEx.funct3)
        {
        case RISCV_BR_BEQ:
            core.extoMem.result = lhs == rhs;
            break;
        case RISCV_BR_BNE:
            core.extoMem.result = lhs != rhs;
            break;
        case RISCV_BR_BLT:
            core.extoMem.result = lhs < rhs;
            break;
        case RISCV_BR_BGE:
            core.extoMem.result = lhs >= rhs;
            break;
        case RISCV_BR_BLTU:
            core.extoMem.result = (ac_int<32, false>)lhs < (ac_int<32, false>)rhs;
            break;
        case RISCV_BR_BGEU:
            core.extoMem.result = (ac_int<32, false>)lhs >= (ac_int<32, false>)rhs;
            break;
        EXDEFAULT();
        }
        core.ctrl.branch[0] = core.extoMem.result;
        core.ctrl.jump_pc[0] = core.dctoEx.pc + core.dctoEx.datac; // cannot be done by fetch...
        break;
    case RISCV_LD:
        core.extoMem.result = lhs + rhs;
        break;
    case RISCV_ST:
        core.extoMem.result = lhs + rhs;
        break;
    case RISCV_OPI:
        switch(core.dctoEx.funct3)
        {
        case RISCV_OPI_ADDI:
            core.extoMem.result = lhs + rhs;
            break;
        case RISCV_OPI_SLTI:
            core.extoMem.result = lhs < rhs;
            break;
        case RISCV_OPI_SLTIU:
            core.extoMem.result = (ac_int<32, false>)lhs < (ac_int<32, false>)rhs;
            break;
        case RISCV_OPI_XORI:
            core.extoMem.result = lhs ^ rhs;
            break;
        case RISCV_OPI_ORI:
            core.extoMem.result = lhs | rhs;
            break;
        case RISCV_OPI_ANDI:
            core.extoMem.result = lhs & rhs;
            break;
        case RISCV_OPI_SLLI: // cast rhs as 5 bits, otherwise generated hardware is 32 bits
            // & shift amount held in the lower 5 bits (riscv spec)
            core.extoMem.result = lhs << (ac_int<5, false>)rhs;
            break;
        case RISCV_OPI_SRI:
            if (core.dctoEx.funct7.slc<1>(5))    //SRAI
                core.extoMem.result = lhs >> (ac_int<5, false>)rhs;
            else                            //SRLI
                core.extoMem.result = (ac_int<32, false>)lhs >> (ac_int<5, false>)rhs;
            break;
        EXDEFAULT();
        }
        break;
    case RISCV_OP:
        if(core.dctoEx.funct7.slc<1>(0))     // M Extension
        {
            ac_int<33, true> mul_reg_a = lhs;
            ac_int<33, true> mul_reg_b = rhs;
            ac_int<66, true> longResult = 0;
            switch (core.dctoEx.funct3)  //Switch case for multiplication operations (RV32M)
            {
            case RISCV_OP_M_MULHSU:
                mul_reg_b[32] = 0;
                break;
            case RISCV_OP_M_MULHU:
                mul_reg_a[32] = 0;
                mul_reg_b[32] = 0;
                break;
        #ifndef __HLS__
            case RISCV_OP_M_DIVU:
                mul_reg_a[32] = 0;
                mul_reg_b[32] = 0;
                break;
            case RISCV_OP_M_REMU:
                mul_reg_a[32] = 0;
                mul_reg_b[32] = 0;
                break;
        #endif
            }
        #ifndef __HLS__
            switch(core.dctoEx.funct3)
            {
            case RISCV_OP_M_MUL:
            case RISCV_OP_M_MULH:
            case RISCV_OP_M_MULHSU:
            case RISCV_OP_M_MULHU:
                longResult = mul_reg_a * mul_reg_b;
                break;
            case RISCV_OP_M_DIV:
            case RISCV_OP_M_DIVU:
                if(mul_reg_b)
                    longResult = mul_reg_a / mul_reg_b;
                else    // divide by 0 => set all bits to 1
                    longResult = -1;
                break;
            case RISCV_OP_M_REM:
            case RISCV_OP_M_REMU:
                if(mul_reg_b)
                    longResult = mul_reg_a % mul_reg_b;
                else    // modulo 0 => result is first operand
                    longResult = mul_reg_a;
                break;
            EXDEFAULT();
            }
        #else
            longResult = mul_reg_a * mul_reg_b;
        #endif
            if(core.dctoEx.funct3 == RISCV_OP_M_MULH || core.dctoEx.funct3 == RISCV_OP_M_MULHSU || core.dctoEx.funct3 == RISCV_OP_M_MULHU)
            {
                core.extoMem.result = longResult.slc<32>(32);
            }
            else
            {
                core.extoMem.result = longResult.slc<32>(0);
            }
        }
        else
        {
            switch(core.dctoEx.funct3)
            {
            case RISCV_OP_ADD:
                if (core.dctoEx.funct7.slc<1>(5))    // SUB
                    core.extoMem.result = lhs - rhs;
                else                            // ADD
                    core.extoMem.result = lhs + rhs;
                break;
            case RISCV_OP_SLL:
                core.extoMem.result = lhs << (ac_int<5, false>)rhs;
                break;
            case RISCV_OP_SLT:
                core.extoMem.result = lhs < rhs;
                break;
            case RISCV_OP_SLTU:
                core.extoMem.result = (ac_int<32, false>)lhs < (ac_int<32, false>)rhs;
                break;
            case RISCV_OP_XOR:
                core.extoMem.result = lhs ^ rhs;
                break;
            case RISCV_OP_SR:
                if(core.dctoEx.funct7.slc<1>(5))     // SRA
                    core.extoMem.result = lhs >> (ac_int<5, false>)rhs;
                else                            // SRL
                    core.extoMem.result = (ac_int<32, false>)lhs >> (ac_int<5, false>)rhs;
                break;
            case RISCV_OP_OR:
                core.extoMem.result = lhs | rhs;
                break;
            case RISCV_OP_AND:
                core.extoMem.result = lhs & rhs;
                break;
            EXDEFAULT();
            }
        }
        break;
    case RISCV_MISC_MEM:    // this does nothing because all memory accesses are ordered and we have only one core
        break;
    case RISCV_SYSTEM:
        switch(core.dctoEx.funct3)
        { // case 0: mret instruction, core.dctoEx.memValue should be 0x302
        case RISCV_SYSTEM_ENV:
        #ifndef __HLS__
            dbglog("Syscall @%06x (%lld)\n", core.dctoEx.pc.to_int(), core.csrs.mcycle.to_int64());
            core.extoMem.result = sim->solveSyscall(lhs, rhs, core.dctoEx.datac, core.dctoEx.datad, core.dctoEx.datae, exit);
        #endif
            break;
            // lhs is from rs1, rhs is from csr
        case RISCV_SYSTEM_CSRRW:
            core.extoMem.datac = rhs;       // written back to csr
            core.extoMem.result = lhs;      // written back to rd
            break;
        case RISCV_SYSTEM_CSRRS:
            core.extoMem.datac = lhs | rhs;
            core.extoMem.result = lhs;
            break;
        case RISCV_SYSTEM_CSRRC:
            core.extoMem.datac = lhs & ((ac_int<32, false>)~rhs);
            core.extoMem.result = lhs;
            break;
        case RISCV_SYSTEM_CSRRWI:
            core.extoMem.datac = rhs;
            core.extoMem.result = lhs;
            break;
        case RISCV_SYSTEM_CSRRSI:
            core.extoMem.datac = lhs | rhs;
            core.extoMem.result = lhs;
            break;
        case RISCV_SYSTEM_CSRRCI:
            core.extoMem.datac = lhs & ((ac_int<32, false>)~rhs);
            core.extoMem.result = lhs;
            break;
        EXDEFAULT();
        }
        break;
    EXDEFAULT();
    }

    core.ctrl.prev_res[0] = core.extoMem.result;

    simul(if(core.dctoEx.pc)
    {
        coredebug("Ex   @%06x   %08x\n", core.dctoEx.pc.to_int(), core.dctoEx.instruction.to_int());
    }
    else
    {
        coredebug("Ex   \n");
    })
}

void do_Mem(Core& core, unsigned int data_memory[DRAM_SIZE]
            #ifndef __HLS__
                , ac_int<64, false>& cycles
            #endif
            )
{
    if(core.ctrl.cachelock)
    {
#ifndef nocache
        if(core.datavalid)
        {
            core.memtoWB.pc = core.extoMem.pc;
            simul(core.memtoWB.instruction = core.extoMem.instruction;)
            core.memtoWB.rd = core.extoMem.rd;
            core.memtoWB.realInstruction = core.extoMem.realInstruction;
            if(!core.writeenable)
                core.memtoWB.result = core.readvalue;
            core.ctrl.cachelock = false;
            core.dcacheenable = false;
            core.writeenable = false;
        }
        else
        {
            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;
        }
#else
        data_memory[core.daddress >> 2] = core.dctrl.valuetowrite;
        core.ctrl.cachelock = false;

        core.memtoWB.realInstruction = core.extoMem.realInstruction;
        simul(core.memtoWB.pc = core.extoMem.pc;
        core.memtoWB.instruction = core.extoMem.instruction;)
        core.memtoWB.rd = core.extoMem.rd;
#endif
    }
    else if(core.ctrl.branch[2])
    {
        gdebug("I    @%06x\n", core.extoMem.pc.to_int());
        core.memtoWB.pc = 0;
        core.extoMem.pc = 0;
        simul(core.memtoWB.instruction = 0;
              core.extoMem.instruction = 0;)

        core.memtoWB.rd = 0;
        core.memtoWB.realInstruction = false;
    }
    else
    {
        switch(core.extoMem.opCode)
        {
        case RISCV_LD:
        {
            core.datasize = core.extoMem.funct3.slc<2>(0);
            core.signenable = !core.extoMem.funct3.slc<1>(2);
#ifndef nocache
            core.daddress = core.extoMem.result;
            core.dcacheenable = true;
            core.writeenable = false;
            core.ctrl.cachelock = true;

            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;
#else
            core.memtoWB.realInstruction = core.extoMem.realInstruction;
            simul(core.memtoWB.pc = core.extoMem.pc;
            core.memtoWB.instruction = core.extoMem.instruction;)
            core.memtoWB.rd = core.extoMem.rd;

            ac_int<32, false> mem_read = data_memory[core.extoMem.result >> 2];
            simul(cycles += MEMORY_READ_LATENCY;)
            formatread(core.extoMem.result, core.datasize, core.signenable, mem_read);
            core.memtoWB.result = mem_read;

            // data                                             size                @address
            coredebug("dR%d  @%06x   %08x   %08x   %s\n", core.datasize.to_int(), core.extoMem.result.to_int(),
                      data_memory[core.extoMem.result >> 2], mem_read.to_int(), core.signenable?"true":"false");
                   // what is in memory                  what is actually read    sign extension
#endif
            simul(if(core.extoMem.result >= 0x11040 && core.extoMem.result < 0x11048)
            {
                fprintf(stderr, "end here? %lld   @%06x   @%06x R%d\n", core.csrs.mcycle.to_int64(), core.extoMem.pc.to_int(), core.extoMem.result.to_int(),
                          core.datasize.to_int());
                core.ctrl.cachelock = false;
                core.memtoWB.result = 1;
            })
            break;
        }
        case RISCV_ST:
        {
            core.datasize = core.extoMem.funct3.slc<2>(0);
            core.signenable = core.extoMem.funct3.slc<1>(2);

            simul(if(core.extoMem.result >= 0x11000 && core.extoMem.result < 0x11040)
                  fprintf(stderr, "end here? %lld   @%06x   @%06x W%d  %08x\n", core.csrs.mcycle.to_int64(), core.extoMem.pc.to_int(), core.extoMem.result.to_int(),
                          core.datasize.to_int(), core.extoMem.datac.to_int());)
#ifndef nocache
            core.daddress = core.extoMem.result;
            core.dcacheenable = true;
            core.writeenable = true;
            core.writevalue = core.extoMem.datac;
            core.ctrl.cachelock = true;

            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;
#else
            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;

            ac_int<32, false> memory_val = data_memory[core.extoMem.result >> 2];
            simul(cycles += MEMORY_READ_LATENCY;)
            formatwrite(core.extoMem.result, core.datasize, memory_val, core.extoMem.datac);

            core.daddress = core.extoMem.result;
            core.dctrl.valuetowrite = memory_val;   // dctrl is not used anyway

            // data                                         size                    @address
            coredebug("dW%d  @%06x   %08x   %08x   %08x\n", core.datasize.to_int(), core.extoMem.result.to_int(),
                      data_memory[core.extoMem.result >> 2], core.extoMem.datac.to_int(), memory_val.to_int());
                   // what was there before                 what we want to write       what is actually written

            core.ctrl.cachelock = true;     // we need one more cycle to write the formatted data
#endif
            break;
        }
        default:
            core.memtoWB.pc = core.extoMem.pc;
            simul(core.memtoWB.instruction = core.extoMem.instruction;)

            core.memtoWB.result = core.extoMem.result;
            //core.memtoWB.CSRid = core.extoMem.memValue;
            core.memtoWB.rescsr = core.extoMem.datac;
            core.memtoWB.csrwb = false;
            core.memtoWB.realInstruction = core.extoMem.realInstruction;
            core.memtoWB.rd = core.extoMem.rd;
            break;
        }
    }

    simul(if(core.extoMem.pc)
    {
        coredebug("Mem  @%06x   %08x\n", core.extoMem.pc.to_int(), core.extoMem.instruction.to_int());
    }
    else
    {
        coredebug("Mem  \n");
    })
}

void doWB(Core& core)
{
    if(core.memtoWB.rd != 0)
    {
        core.REG[core.memtoWB.rd] = core.memtoWB.result;
    }

    if(core.memtoWB.csrwb)     // condition should be more precise
    {
        fprintf(stderr, "Writing %08x in CSR @%03x   @%06x\n", core.memtoWB.rescsr.to_int(), core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());

        if(core.memtoWB.CSRid.slc<2>(8) == 3)
        {
            if(core.memtoWB.CSRid.slc<2>(10) == 0)
            {
                if(core.memtoWB.CSRid.slc<1>(6) == 0)  // 0x30X
                {
                    switch(core.memtoWB.CSRid.slc<3>(0))
                    {
                    case 0:
                        core.csrs.mstatus = core.memtoWB.rescsr;
                        break;
                    case 1:
                        core.csrs.misa = core.memtoWB.rescsr;
                        break;
                    case 2:
                        core.csrs.medeleg = core.memtoWB.rescsr;
                        break;
                    case 3:
                        core.csrs.mideleg = core.memtoWB.rescsr;
                        break;
                    case 4:
                        core.csrs.mie = core.memtoWB.rescsr;
                        break;
                    case 5:
                        core.csrs.mtvec = core.memtoWB.rescsr;
                        break;
                    case 6:
                        core.csrs.mcounteren = core.memtoWB.rescsr;
                        break;
                    default:
                        dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
                        break;
                    }
                }
                else                        // 0x34X
                {
                    switch(core.memtoWB.CSRid.slc<3>(0))
                    {
                    case 0:
                        core.csrs.mscratch = core.memtoWB.rescsr;
                        break;
                    case 1:
                        core.csrs.mepc = core.memtoWB.rescsr;
                        break;
                    case 2:
                        core.csrs.mcause = core.memtoWB.rescsr;
                        break;
                    case 3:
                        core.csrs.mtval = core.memtoWB.rescsr;
                        break;
                    case 4:
                        core.csrs.mip = core.memtoWB.rescsr;
                        break;
                    default:
                        dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
                        break;
                    }
                }
            }
            else if(core.memtoWB.CSRid.slc<2>(10) == 2)
            {
                ac_int<2, false> foo = 0;
                foo.set_slc(0, core.memtoWB.CSRid.slc<1>(1));
                foo.set_slc(1, core.memtoWB.CSRid.slc<1>(7));
                switch(foo)
                {
                case 0:
                    core.csrs.mcycle.slc<32>(0) = core.memtoWB.rescsr;
                    break;
                case 1:
                    core.csrs.minstret.slc<32>(0) = core.memtoWB.rescsr;
                    break;
                case 2:
                    core.csrs.mcycle.slc<32>(32) = core.memtoWB.rescsr;
                    break;
                case 3:
                    core.csrs.minstret.slc<32>(32) = core.memtoWB.rescsr;
                    break;
                }
            }
            else if(core.memtoWB.CSRid.slc<2>(10) == 3)
            {
                dbgassert(false, "Read only CSR %03x", core.memtoWB.CSRid);
            }
            else
            {
                dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
            }
        }
        else
        {
            dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
        }
    }

    core.csrs.minstret += core.memtoWB.realInstruction;
    simul(
    if(core.memtoWB.realInstruction)
    {
        coredebug("\nWB   @%06x   %08x   (%lld)\n", core.memtoWB.pc.to_int(), core.memtoWB.instruction.to_int(), core.csrs.minstret.to_int64());
    }
    else
    {
        coredebug("\nWB   \n");
    })

}

// writing all the init stuff in {} is messy
// and lead easily to error
void coreinit(Core& core, ac_int<32, false> startpc)
{
    core.ctrl.init = true;

    core.pc = startpc;
    core.ftoDC.instruction = simul(core.dctoEx.instruction =
    core.extoMem.instruction = core.memtoWB.instruction =) 0x13;

    core.ctrl.prev_opCode[2] = core.ctrl.prev_opCode[1] = RISCV_OPI;
    core.dctoEx.opCode = core.extoMem.opCode = RISCV_OPI;

    core.REG[2] = STACK_INIT;

#if Policy == RP_RANDOM
    core.ictrl.setctrl.policy = 0xF2D4B698;
    core.dctrl.setctrl.policy = 0xF2D4B698;
#endif
}

template<unsigned int hartid>
void doCore(ac_int<32, false> startpc, bool &exit,
            unsigned int im[DRAM_SIZE], unsigned int dm[DRAM_SIZE],
            unsigned int cim[Sets][Blocksize][Associativity], unsigned int cdm[Sets][Blocksize][Associativity],
            ac_int<IWidth, false> memictrl[Sets], ac_int<DWidth, false> memdctrl[Sets]
        #ifndef __HLS__
            , Simulator* sim
        #endif
            )
{
    static Core core;

    if(!core.ctrl.init)
    {
        coreinit(core, startpc);

        simul(sim->setCore(&core, memdctrl, (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(cdm)));)
    }

    core.csrs.mcycle += 1;

    doWB(core);
    simul(coredebug("%lld ", core.csrs.mcycle.to_int64());
    for(int i=0; i<32; i++)
    {
        if(core.REG[i])
            coredebug("%d:%08x ", i, (int)core.REG[i]);
    }
    coredebug("\n");)

    do_Mem(core , dm
       #ifndef __HLS__
           , core.csrs.mcycle
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
        Ft(core, im
   #ifndef __HLS__
          , core.csrs.mcycle
   #endif
          );

        if(core.ctrl.prev_opCode[2] == RISCV_LD)
            core.ctrl.prev_res[2] = core.memtoWB.result;    // store the loaded value, not the address
            // this works because we know that we can't have lw a5, x   addi a0, a5, 1
            // there's always a nop between load and next use of value
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


#ifndef nocache
    // cache should maybe be all the way up or down
    // cache down generates less hardware and is less cycle costly(20%?)
    // but cache up has a slightly better critical path
    dcache(core.dctrl, memdctrl, dm, cdm, core.daddress, core.datasize, core.signenable, core.dcacheenable,
           core.writeenable, core.writevalue, core.readvalue, core.datavalid);
    icache(core.ictrl, memictrl, im, cim, core.iaddress, core.cachepc, core.instruction, core.insvalid);
#endif


    // riscv specification v2.2 p11
    dbgassert((core.pc.to_int() & 3) == 0, "Misaligned instruction @%06x\n",
               core.pc.to_int());

}

void doStep(ac_int<32, false> startpc, bool &exit,
            unsigned int im[DRAM_SIZE], unsigned int dm[DRAM_SIZE],
            unsigned int cim[Sets][Blocksize][Associativity], unsigned int cdm[Sets][Blocksize][Associativity],
            ac_int<IWidth, false> memictrl[Sets], ac_int<DWidth, false> memdctrl[Sets]
        #ifndef __HLS__
            , Simulator* sim
        #endif
            )
{
    simul(uint64_t oldcycles = core.csrs.mcycle;)

    doCore<0>(startpc, exit, im, dm,
              cim, cdm, memictrl, memdctrl
          #ifndef __HLS__
              , sim
          #endif
              );

    // doCore<1>(...)
    // directory cache control


    simul(
    int M = MEMORY_READ_LATENCY>MEMORY_WRITE_LATENCY?MEMORY_READ_LATENCY:MEMORY_WRITE_LATENCY;
    if(oldcycles + M < core.csrs.mcycle)
    {
        core.csrs.mcycle = oldcycles + M; // we cannot step slower than the worst latency
    }
    )
}
