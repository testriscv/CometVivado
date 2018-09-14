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

void Ft(Core& core
    #ifdef nocache
        , unsigned int im[DRAM_SIZE]
        #ifndef __HLS__
            , ac_int<64, false>& cycles
        #endif
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
        if(core.ireply.insvalid && core.ireply.cachepc == core.pc)
        {
            core.ftoDC.instruction = core.ireply.instruction;
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

    core.irequest.address = core.pc;
    gdebug("%06x\n", core.irequest.address.to_int());

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
    ac_int<7, false> opCode = instruction.slc<7>(0);    // could be reduced to 5 bits because 1:0 is always 11
    // cannot reduce opCode to 5 bits with modelsim (x propagation...)

    bool csr = false;
    ac_int<12, false> CSRid = instruction.slc<12>(20);

    ac_int<32, true> lhs = 0;
    ac_int<32, true> rhs = 0;
    ac_int<32, true> datac = 0;
    bool realInstruction = true;

    bool forward_rs1 = false;
    bool forward_rs2 = false;
    bool forward_datac = false;
    bool forward_ex_or_mem_rs1 = false;
    bool forward_ex_or_mem_rs2 = false;
    bool forward_ex_or_mem_datac = false;

    bool external = false;
    MultiCycleOperation::MultiCycleOperation op = MultiCycleOperation::NONE;


    // prev_rds[1] is ex, prev_rds[2] is mem
    forward_rs1 = (rs1 != 0) && (core.ctrl.prev_rds[1] == rs1 || core.ctrl.prev_rds[2] == rs1);
    forward_rs2 = (rs2 != 0) && (core.ctrl.prev_rds[1] == rs2 || core.ctrl.prev_rds[2] == rs2);
    forward_ex_or_mem_rs1 = core.ctrl.prev_rds[1] == rs1;
    forward_ex_or_mem_rs2 = core.ctrl.prev_rds[1] == rs2;

    core.ctrl.freeze_fetch = 0;
#ifdef __HLS__
    exit = false;
#endif

#ifndef COMET_NO_BR
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
    else
#endif
#if !defined(COMET_NO_JAL) || !defined(COMET_NO_JALR)
    if(core.ctrl.lock)
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
    else
#endif
#ifndef COMET_NO_LD
    if(core.ctrl.prev_opCode[1] == RISCV_LD && (core.ctrl.prev_rds[1] == rs1
            || core.ctrl.prev_rds[1] == rs2))   // is this really necessary, because we are locked anyway by mem stage
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
    else
#endif
    if(core.dctoEx.external)    // bubble because we need one more cycle to write forward registers
    {
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
        dbgassert(instruction.slc<2>(0) == 3, "Instruction lower bits are not 0b11, illegal instruction @%06x : %08x\n",
                  pc.to_int(), instruction.to_int());

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
        #ifndef COMET_NO_OP
        case RISCV_OP:
            #ifndef COMET_NO_M
            if(funct7.slc<1>(0))    // M extension
            {
                if(funct3.slc<1>(2))    // DIV, DIVU, REM, REMU
                {
                    switch(funct3.slc<2>(0))
                    {
                    case 0: // DIV
                        op = MultiCycleOperation::DIV;
                        break;
                    case 1: // DIVU
                        op = MultiCycleOperation::DIVU;
                        break;
                    case 2: // REM
                        op = MultiCycleOperation::REM;
                        break;
                    case 3: // REMU
                        op = MultiCycleOperation::REMU;
                        break;
                    default:
                        dbgassert(false, "Impossible case @%06x\n", pc.to_int());
                        break;
                    }
                    external = true;

                    //dbglog("DIV/U or REM/U operation @%06x\n", pc);
                }
            }
            #endif
            break;
        #endif
        #ifndef COMET_NO_ATOMIC
        case RISCV_ATOMIC:
            dbgassert(opCode != RISCV_ATOMIC, "Atomic operation not implemented yet @%06x   %08x\n",
                      pc.to_int(), instruction.to_int());
            break;
        #endif
        // S-type instruction
        // use rs2 rs1 funct3 opCode from R-type
        // 12 bits immediate
        #ifndef COMET_NO_ST
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
        #endif
        // B-type instruction
        // use rs2 rs1 funct3 opCode from R-type
        // 13 bits immediate
        #ifndef COMET_NO_BR
        case RISCV_BR:
            immediate.set_slc(0, (ac_int<1, true>)0);
            immediate.set_slc(1, instruction.slc<4>(8));
            immediate.set_slc(5, instruction.slc<6>(25));
            immediate.set_slc(11, instruction.slc<1>(7));

            datac = immediate;
            rd = 0;
            break;
        #endif
        // J-type instruction
        // use rd opCode from R-type
        // 20 bits immediate
        #ifndef COMET_NO_JAL
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
        #endif
        // U-type instruction
        // use rd opCode from R-type
        // 20 bits immediate
        #ifndef COMET_NO_LUI
        case RISCV_LUI:
            immediate.set_slc(0, (ac_int<12, true>)0);
            immediate.set_slc(12, instruction.slc<19>(12));
            lhs = immediate;
            forward_rs1 = false;
            forward_rs2 = false;
            break;
        #endif
        #ifndef COMET_NO_AUIPC
        case RISCV_AUIPC:
            immediate.set_slc(0, (ac_int<12, true>)0);
            immediate.set_slc(12, instruction.slc<19>(12));
            lhs = pc;
            rhs = immediate;
            forward_rs1 = false;
            forward_rs2 = false;
            break;
        #endif
        // I-type instruction
        // use rs1 funct3 rd opCode from R-type
        // 12 bits immediate
        #ifndef COMET_NO_LD
        case RISCV_LD:
            immediate.set_slc(0, instruction.slc<11>(20));
            rhs = immediate;
            forward_rs2 = false;

            break;
        #endif
        #ifndef COMET_NO_OPI
        case RISCV_OPI:
            immediate.set_slc(0, instruction.slc<11>(20));
            // no need to test for SLLI or SRAI because only 5 bits are used in EX stage
            rhs = immediate;
            forward_rs2 = false;

            break;
        #endif
        #ifndef COMET_NO_JALR
        case RISCV_JALR:
            immediate.set_slc(0, instruction.slc<11>(20));
            rhs = immediate;
            forward_rs2 = false;
            datac = core.ftoDC.nextpc;

            // lock DC for 3 cycles, until we get the real next instruction
            core.ctrl.lock = 3;
            break;
        #endif
        #ifndef COMET_NO_MISC_MEM
        case RISCV_MISC_MEM:
            immediate.set_slc(0, instruction.slc<11>(20));
            rhs = immediate;
            forward_rs2 = false;
            break;
        #endif
        default:
            dbgassert(false, "Error : Unknown operation in DC stage : @%06x	%08x\n", pc.to_int(), instruction.to_int());
            break;
        #ifndef COMET_NO_SYSTEM
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
            #ifndef COMET_NO_CSR
            else simul(if(funct3 != 0x4))
            {
                if(funct3.slc<1>(2))    // CSR + CSRid
                    rhs = rs1;
                else                    // CSR + reg[rs1]
                    rhs = lhs;
                // handle the case for rd = 0 for CSRRW
                // handle WIRI/WARL/etc.
                fprintf(stderr, "%03x :     %x  %x  %x  %x\n", (int)CSRid, (int)CSRid.slc<2>(10), (int)CSRid.slc<2>(8),
                        (int)CSRid.slc<1>(6), (int)CSRid.slc<3>(0));
                csr = true;
                if(CSRid.slc<2>(8) == 3)
                {
                    if(CSRid.slc<2>(10) == 0)
                    {
                        if(CSRid.slc<1>(6) == 0)  // 0x30X
                        {
                            switch(CSRid.slc<3>(0))
                            {
                            case 0:
                                lhs = core.csrs.mstatus;
                                fprintf(stderr, "CSR read mstatus @%06x  %08x\n", pc.to_int(), core.csrs.mstatus.to_int());
                                break;
                            case 1:
                                lhs = core.csrs.misa;
                                fprintf(stderr, "CSR read misa @%06x  %08x\n", pc.to_int(), core.csrs.misa.to_int());
                                break;
                            case 2:
                                lhs = core.csrs.medeleg;
                                fprintf(stderr, "CSR read medeleg @%06x  %08x\n", pc.to_int(), core.csrs.medeleg.to_int());
                                break;
                            case 3:
                                lhs = core.csrs.mideleg;
                                fprintf(stderr, "CSR read mideleg @%06x  %08x\n", pc.to_int(), core.csrs.mideleg.to_int());
                                break;
                            case 4:
                                lhs = core.csrs.mie;
                                fprintf(stderr, "CSR read mie @%06x  %08x\n", pc.to_int(), core.csrs.mie.to_int());
                                break;
                            case 5:
                                lhs = core.csrs.mtvec;
                                fprintf(stderr, "CSR read mtvec @%06x  %08x\n", pc.to_int(), core.csrs.mtvec.to_int());
                                break;
                            case 6:
                                lhs = core.csrs.mcounteren;
                                fprintf(stderr, "CSR read mcounteren @%06x  %08x\n", pc.to_int(), core.csrs.mcounteren.to_int());
                                break;
                            default:
                                dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", CSRid.to_int(), pc.to_int());
                                break;
                            }
                        }
                        else                        // 0x34X
                        {
                            switch(CSRid.slc<3>(0))
                            {
                            case 0:
                                lhs = core.csrs.mscratch;
                                fprintf(stderr, "CSR read mscratch @%06x  %08x\n", pc.to_int(), core.csrs.mscratch.to_int());
                                break;
                            case 1:
                                lhs = core.csrs.mepc;
                                fprintf(stderr, "CSR read mepc @%06x  %08x\n", pc.to_int(), core.csrs.mepc.to_int());
                                break;
                            case 2:
                                lhs = core.csrs.mcause;
                                fprintf(stderr, "CSR read mcause @%06x  %08x\n", pc.to_int(), core.csrs.mcause.to_int());
                                break;
                            case 3:
                                lhs = core.csrs.mtval;
                                fprintf(stderr, "CSR read mtval @%06x  %08x\n", pc.to_int(), core.csrs.mtval.to_int());
                                break;
                            case 4:
                                lhs = core.csrs.mip;
                                fprintf(stderr, "CSR read mip @%06x  %08x\n", pc.to_int(), core.csrs.mip.to_int());
                                break;
                            default:
                                dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", CSRid.to_int(), pc.to_int());
                                break;
                            }
                        }
                    }
                    else if(CSRid.slc<2>(10) == 2)
                    {
                        ac_int<2, false> foo = 0;
                        foo.set_slc(0, CSRid.slc<1>(1));
                        foo.set_slc(1, CSRid.slc<1>(7));
                        switch(foo)
                        {
                        case 0:
                            lhs = core.csrs.mcycle.slc<32>(0);
                            fprintf(stderr, "CSR read mcycle @%06x  %08x\n", pc.to_int(), core.csrs.mcycle.to_int());
                            break;
                        case 1:
                            lhs = core.csrs.minstret.slc<32>(0);
                            fprintf(stderr, "CSR read minstret @%06x  %08x\n", pc.to_int(), core.csrs.minstret.to_int());
                            break;
                        case 2:
                            lhs = core.csrs.mcycle.slc<32>(32);
                            fprintf(stderr, "CSR read mcycle @%06x  %08x\n", pc.to_int(), core.csrs.mcycle.to_int());
                            break;
                        case 3:
                            lhs = core.csrs.minstret.slc<32>(32);
                            fprintf(stderr, "CSR read minstret @%06x  %08x\n", pc.to_int(), core.csrs.minstret.to_int());
                            break;
                        }
                    }
                    else if(CSRid.slc<2>(10) == 3)
                    {
                        switch(CSRid.slc<2>(0))
                        {
                        case 1:
                            lhs = core.csrs.mvendorid;
                            fprintf(stderr, "CSR read mvendorid @%06x  %08x\n", pc.to_int(), core.csrs.mvendorid.to_int());
                            break;
                        case 2:
                            lhs = core.csrs.marchid;
                            fprintf(stderr, "CSR read marchid @%06x  %08x\n", pc.to_int(), core.csrs.marchid.to_int());
                            break;
                        case 3:
                            lhs = core.csrs.mimpid;
                            fprintf(stderr, "CSR read mimpid @%06x  %08x\n", pc.to_int(), core.csrs.mimpid.to_int());
                            break;
                        case 0:
                            lhs = hartid;
                            fprintf(stderr, "CSR read mhartid @%06x  %08x\n", pc.to_int(), hartid);
                            break;
                        }
                    }
                    else
                    {
                        dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", CSRid.to_int(), pc.to_int());
                    }
                }
                else
                {
                    dbgassert(false, "Unknown CSR id : @%03x     @%06x\n", CSRid.to_int(), pc.to_int());
                }
                fprintf(stderr, "Reading %08x in CSR @%03x    @%06x\n", lhs.to_int(), CSRid.to_int(), pc.to_int());
                //rhs will contain core.REG[rs1]
            }
            #endif // COMET_NO_CSR
            simul(else dbgassert(false, "Unknown operation @%06x    %08x\n",
                                        pc.to_int(), instruction.to_int()));
            break;
        #endif  // COMET_NO_SYSTEM
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

    core.dctoEx.external = external;
    core.dctoEx.op = op;

    core.dctoEx.csr = csr;
    core.dctoEx.CSRid = CSRid;

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
#ifndef __HLS__
    if(core.ctrl.prev_opCode[2] == RISCV_BR && core.ctrl.branch[1])
        return; // prevent counting operation executed after a branch is taken
#endif

    core.extoMem.pc = core.dctoEx.pc;
    simul(core.extoMem.instruction = core.dctoEx.instruction;)
    core.extoMem.opCode = core.dctoEx.opCode;
    core.extoMem.rd = core.dctoEx.rd;

    core.extoMem.realInstruction = core.dctoEx.realInstruction;
    core.extoMem.funct3 = core.dctoEx.funct3;

    core.extoMem.csr = core.dctoEx.csr;
    core.extoMem.CSRid = core.dctoEx.CSRid;

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
        core.extoMem.datac = core.dctoEx.datac;

    
    core.mcop.op = core.dctoEx.op;
    core.extoMem.external = core.dctoEx.external;
    core.mcop.lhs = lhs;
    core.mcop.rhs = rhs;
    //core.mcop.rd = core.dctoEx.rd;
    core.mcop.pc = core.dctoEx.pc;
    if(core.dctoEx.external)
    {
        gdebug("Starting external op @%06x (%lld %lld)\n", core.dctoEx.pc.to_int(),
               core.csrs.minstret.to_int64(), core.csrs.mcycle.to_int64());
    }
    else
    {
        // switch must be in the else, otherwise external op may trigger default case
        switch(core.dctoEx.opCode)
        {
        #ifndef COMET_NO_LUI
        case RISCV_LUI:
            core.extoMem.result = lhs;
            simul(sim->coredata.lui++;)
            break;
        #endif
        #ifndef COMET_NO_AUIPC
        case RISCV_AUIPC:
            core.extoMem.result = lhs + rhs;
            simul(sim->coredata.auipc++;)
            break;
        #endif
        #ifndef COMET_NO_JAL
        case RISCV_JAL:
            core.ctrl.jump_pc[0] = lhs + rhs;
            core.extoMem.result = core.dctoEx.datac;   // pc + 4
            simul(sim->coredata.jal++;)
            break;
        #endif
        #ifndef COMET_NO_JALR
        case RISCV_JALR:
            core.ctrl.jump_pc[0] = (lhs + rhs) & 0xfffffffe;   // lsb must be zeroed (cf spec)
            core.extoMem.result = core.dctoEx.datac;   // pc + 4
            simul(sim->coredata.jalr++;)
            break;
        #endif
        #ifndef COMET_NO_BR
        case RISCV_BR:
            switch(core.dctoEx.funct3)
            {
            #ifndef COMET_NO_BR_BEQ
            case RISCV_BR_BEQ:
                core.extoMem.result = lhs == rhs;
                simul(sim->coredata.br[0]++;)
                break;
            #endif
            #ifndef COMET_NO_BR_BNE
            case RISCV_BR_BNE:
                core.extoMem.result = lhs != rhs;
                simul(sim->coredata.br[1]++;)
                break;
            #endif
            #ifndef COMET_NO_BR_BLT
            case RISCV_BR_BLT:
                core.extoMem.result = lhs < rhs;
                simul(sim->coredata.br[2]++;)
                break;
            #endif
            #ifndef COMET_NO_BR_BGE
            case RISCV_BR_BGE:
                core.extoMem.result = lhs >= rhs;
                simul(sim->coredata.br[3]++;)
                break;
            #endif
            #ifndef COMET_NO_BR_BLTU
            case RISCV_BR_BLTU:
                core.extoMem.result = (ac_int<32, false>)lhs < (ac_int<32, false>)rhs;
                simul(sim->coredata.br[4]++;)
                break;
            #endif
            #ifndef COMET_NO_BR_BGEU
            case RISCV_BR_BGEU:
                core.extoMem.result = (ac_int<32, false>)lhs >= (ac_int<32, false>)rhs;
                simul(sim->coredata.br[5]++;)
                break;
            #endif
            EXDEFAULT();
            }
            core.ctrl.branch[0] = core.extoMem.result;
            core.ctrl.jump_pc[0] = core.dctoEx.pc + core.dctoEx.datac; // cannot be done by fetch...
            break;
        #endif
        #ifndef COMET_NO_LD
        case RISCV_LD:
            core.extoMem.result = lhs + rhs;
            simul(sim->coredata.ld++;)
            break;
        #endif
        #ifndef COMET_NO_ST
        case RISCV_ST:
            core.extoMem.result = lhs + rhs;
            simul(sim->coredata.st++;)
            break;
        #endif
        #ifndef COMET_NO_OPI
        case RISCV_OPI:
            switch(core.dctoEx.funct3)
            {
            #ifndef COMET_NO_OPI_ADDI
            case RISCV_OPI_ADDI:
                core.extoMem.result = lhs + rhs;
                simul(sim->coredata.addi+=core.dctoEx.realInstruction;
                      sim->coredata.bulle++;)

                break;
            #endif
            #ifndef COMET_NO_OPI_SLTI
            case RISCV_OPI_SLTI:
                core.extoMem.result = lhs < rhs;
                simul(sim->coredata.slti++;)
                break;
            #endif
            #ifndef COMET_NO_OPI_SLTIU
            case RISCV_OPI_SLTIU:
                core.extoMem.result = (ac_int<32, false>)lhs < (ac_int<32, false>)rhs;
                simul(sim->coredata.sltiu++;)
                break;
            #endif
            #ifndef COMET_NO_OPI_XORI
            case RISCV_OPI_XORI:
                core.extoMem.result = lhs ^ rhs;
                simul(sim->coredata.xori++;)
                break;
            #endif
            #ifndef COMET_NO_OPI_ORI
            case RISCV_OPI_ORI:
                core.extoMem.result = lhs | rhs;
                simul(sim->coredata.ori++;)
                break;
            #endif
            #ifndef COMET_NO_OPI_ANDI
            case RISCV_OPI_ANDI:
                core.extoMem.result = lhs & rhs;
                simul(sim->coredata.andi++;)
                break;
            #endif
            #ifndef COMET_NO_OPI_SLLI
            case RISCV_OPI_SLLI: // cast rhs as 5 bits, otherwise generated hardware is 32 bits
                // & shift amount held in the lower 5 bits (riscv spec)
                core.extoMem.result = lhs << (ac_int<5, false>)rhs;
                simul(sim->coredata.slli++;)
                break;
            #endif
            #ifndef COMET_NO_OPI_SRI
            case RISCV_OPI_SRI:
                if (core.dctoEx.funct7.slc<1>(5))    //SRAI
                {
                #ifndef COMET_NO_OPI_SRAI
                    core.extoMem.result = lhs >> (ac_int<5, false>)rhs;
                    simul(sim->coredata.srai++;)
                #endif
                }
                else                                //SRLI
                {
                #ifndef COMET_NO_OPI_SRLI
                    core.extoMem.result = (ac_int<32, false>)lhs >> (ac_int<5, false>)rhs;
                    simul(sim->coredata.srli++;)
                #endif
                }
                break;
            #endif  // OPI_SRI
            EXDEFAULT();
            }
            break;
        #endif      // OPI
        #ifndef COMET_NO_OP
        case RISCV_OP:
            #ifndef COMET_NO_M
            if(core.dctoEx.funct7.slc<1>(0))     // M Extension
            {
                ac_int<33, true> mul_reg_a = lhs;
                ac_int<33, true> mul_reg_b = rhs;
                ac_int<66, true> longResult = 0;
                switch (core.dctoEx.funct3)  //Switch case for multiplication operations (RV32M)
                {
                #ifndef COMET_NO_OP_M_MULHSU
                case RISCV_OP_M_MULHSU:
                    mul_reg_b[32] = 0;
                    simul(sim->coredata.mul[3]++;)
                    break;
                #endif
                #ifndef COMET_NO_OP_M_MULHU
                case RISCV_OP_M_MULHU:
                    mul_reg_a[32] = 0;
                    mul_reg_b[32] = 0;
                    simul(sim->coredata.mul[2]++;)
                    break;
                #endif
            #ifndef __HLS__
                #ifndef COMET_NO_OP_M_MULH
                case RISCV_OP_M_MULH:
                    simul(sim->coredata.mul[1]++;)
                    break;
                #endif
                #ifndef COMET_NO_OP_M_MUL
                case RISCV_OP_M_MUL:
                    simul(sim->coredata.mul[0]++;)
                    break;
                #endif
            #endif
                EXDEFAULT();
                }
            #if !defined(COMET_NO_OP_M_MUL) || !defined(COMET_NO_OP_M_MULH) || !defined(COMET_NO_OP_M_MULHSU) || !defined(COMET_NO_OP_M_MULHU)
                longResult = mul_reg_a * mul_reg_b;
            #endif
            #if !defined(COMET_NO_OP_M_MULH) || !defined(COMET_NO_OP_M_MULHSU) || !defined(COMET_NO_OP_M_MULHU)
                if(core.dctoEx.funct3 == RISCV_OP_M_MULH || core.dctoEx.funct3 == RISCV_OP_M_MULHSU || core.dctoEx.funct3 == RISCV_OP_M_MULHU)
                {
                    core.extoMem.result = longResult.slc<32>(32);
                }
                else
            #endif
                {
                    core.extoMem.result = longResult.slc<32>(0);
                }
            }
            else
            #endif  // COMET_NO_M
            {
                switch(core.dctoEx.funct3)
                {
                #if !defined(COMET_NO_OP_SUB) || !defined(COMET_NO_OP_ADD)
                case RISCV_OP_ADD:
                    if (core.dctoEx.funct7.slc<1>(5))   // SUB
                    {
                    #ifndef COMET_NO_OP_SUB
                        core.extoMem.result = lhs - rhs;
                        simul(sim->coredata.sub++;)
                    #endif
                    }
                    else                                // ADD
                    {
                    #ifndef COMET_NO_OP_ADD
                        core.extoMem.result = lhs + rhs;
                        simul(sim->coredata.add++;)
                    #endif
                    }
                    break;
                #endif
                #ifndef COMET_NO_OP_SLL
                case RISCV_OP_SLL:
                    core.extoMem.result = lhs << (ac_int<5, false>)rhs;
                    simul(sim->coredata.sll++;)
                    break;
                #endif
                #ifndef COMET_NO_OP_SLT
                case RISCV_OP_SLT:
                    core.extoMem.result = lhs < rhs;
                    simul(sim->coredata.slt++;)
                    break;
                #endif
                #ifndef COMET_NO_OP_SLTU
                case RISCV_OP_SLTU:
                    core.extoMem.result = (ac_int<32, false>)lhs < (ac_int<32, false>)rhs;
                    simul(sim->coredata.sltu++;)
                    break;
                #endif
                #ifndef COMET_NO_OP_XOR
                case RISCV_OP_XOR:
                    core.extoMem.result = lhs ^ rhs;
                    simul(sim->coredata.opxor++;)
                    break;
                #endif
                #ifndef COMET_NO_OP_SR
                case RISCV_OP_SR:
                    if(core.dctoEx.funct7.slc<1>(5))    // SRA
                    {
                    #ifndef COMET_NO_OP_SRA
                        core.extoMem.result = lhs >> (ac_int<5, false>)rhs;
                        simul(sim->coredata.sra++;)
                    #endif
                    }
                    else                                // SRL
                    {
                    #ifndef COMET_NO_OP_SRL
                        core.extoMem.result = (ac_int<32, false>)lhs >> (ac_int<5, false>)rhs;
                        simul(sim->coredata.srl++;)
                    #endif
                    }
                    break;
                #endif
                #ifndef COMET_NO_OP_OR
                case RISCV_OP_OR:
                    core.extoMem.result = lhs | rhs;
                    simul(sim->coredata.opor++;)
                    break;
                #endif
                #ifndef COMET_NO_OP_AND
                case RISCV_OP_AND:
                    core.extoMem.result = lhs & rhs;
                    simul(sim->coredata.opand++;)
                    break;
                #endif
                EXDEFAULT();
                }
            }
            break;
        #endif  // COMET_NO_OP
        #ifndef COMET_NO_MISC_MEM
        case RISCV_MISC_MEM:    // this does nothing because all memory accesses are ordered and we have only one core
            simul(sim->coredata.misc_mem++;)
            break;
        #endif
        #ifndef COMET_NO_SYSTEM
        case RISCV_SYSTEM:
            switch(core.dctoEx.funct3)
            { // case 0: mret instruction, core.dctoEx.memValue should be 0x302
            #ifndef COMET_NO_SYSTEM_ENV
            case RISCV_SYSTEM_ENV:
            #ifndef __HLS__
                dbglog("Syscall @%06x (%lld)\n", core.dctoEx.pc.to_int(), core.csrs.mcycle.to_int64());
                core.extoMem.result = sim->solveSyscall(lhs, rhs, core.dctoEx.datac, core.dctoEx.datad, core.dctoEx.datae, exit);
                simul(sim->coredata.ecall++;)
            #endif
                break;
            #endif
            #ifndef COMET_NO_CSR
            #ifndef COMET_NO_SYSTEM_CSRRW
            case RISCV_SYSTEM_CSRRW:    // lhs is from csr, rhs is from reg[rs1]
                core.extoMem.datac = rhs;       // written back to csr
                core.extoMem.result = lhs;      // written back to rd
                simul(sim->coredata.csrrw++;)
                break;
            #endif
            #ifndef COMET_NO_SYSTEM_CSRRS
            case RISCV_SYSTEM_CSRRS:
                core.extoMem.datac = lhs | rhs;
                core.extoMem.result = lhs;
                simul(sim->coredata.csrrs++;)
                break;
            #endif
            #ifndef COMET_NO_SYSTEM_CSRRC
            case RISCV_SYSTEM_CSRRC:
                core.extoMem.datac = lhs & ((ac_int<32, false>)~rhs);
                core.extoMem.result = lhs;
                simul(sim->coredata.csrrc++;)
                break;
            #endif
            #ifndef COMET_NO_SYSTEM_CSRRWI
            case RISCV_SYSTEM_CSRRWI:
                core.extoMem.datac = rhs;
                core.extoMem.result = lhs;
                simul(sim->coredata.csrrwi++;)
                break;
            #endif
            #ifndef COMET_NO_SYSTEM_CSRRSI
            case RISCV_SYSTEM_CSRRSI:
                core.extoMem.datac = lhs | rhs;
                core.extoMem.result = lhs;
                simul(sim->coredata.csrrsi++;)
                break;
            #endif
            #ifndef COMET_NO_SYSTEM_CSRRCI
            case RISCV_SYSTEM_CSRRCI:
                core.extoMem.datac = lhs & ((ac_int<32, false>)~rhs);
                core.extoMem.result = lhs;
                simul(sim->coredata.csrrci++;)
                break;
            #endif
            #endif // COMET_NO_CSR
            EXDEFAULT();
            }
            break;
        #endif  // COMET_NO_SYSTEM
        EXDEFAULT();
        }
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

void do_Mem(Core& core
        #ifdef nocache
            , unsigned int data_memory[DRAM_SIZE]
            #ifndef __HLS__
                , ac_int<64, false>& cycles
            #endif
        #endif
            )
{
    if(core.extoMem.external)
    {
        if(core.mcres.done)
        {
            core.ctrl.cachelock = false;
            core.memtoWB.pc = core.extoMem.pc;
            simul(core.memtoWB.instruction = core.extoMem.instruction;)
            core.memtoWB.result = core.mcres.res;
            core.memtoWB.rd = core.extoMem.rd;
            core.memtoWB.realInstruction = core.extoMem.realInstruction;
            core.memtoWB.csr = core.extoMem.csr;
            core.memtoWB.CSRid = core.extoMem.CSRid;
            core.memtoWB.rescsr = core.extoMem.datac;
            gdebug("external operation finished @%06x\n", core.extoMem.pc.to_int());
        }
        else
        {
            core.ctrl.cachelock = true;
            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;
            core.memtoWB.csr = false;
        }
    }
    else if(core.ctrl.cachelock)
    {
#ifndef nocache
        if(core.dreply.datavalid)
        {
            core.memtoWB.pc = core.extoMem.pc;
            simul(core.memtoWB.instruction = core.extoMem.instruction;)
            core.memtoWB.rd = core.extoMem.rd;
            core.memtoWB.realInstruction = core.extoMem.realInstruction;
            if(!core.drequest.writeenable)
                core.memtoWB.result = core.dreply.readvalue;
            core.ctrl.cachelock = false;
            core.drequest.dcacheenable = false;
            core.drequest.writeenable = false;
            core.memtoWB.rescsr = core.extoMem.datac;
            core.memtoWB.csr = core.extoMem.csr;
        }
        else
        {
            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;
            core.memtoWB.csr = false;
        }
#else
        data_memory[core.drequest.address >> 2] = core.drequest.writevalue;
        core.ctrl.cachelock = false;

        core.memtoWB.realInstruction = core.extoMem.realInstruction;
        simul(core.memtoWB.pc = core.extoMem.pc;
        core.memtoWB.instruction = core.extoMem.instruction;)
        core.memtoWB.rd = core.extoMem.rd;
        core.memtoWB.csr = core.extoMem.csr;
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
        core.memtoWB.csr = false;
    }
    else
    {
        switch(core.extoMem.opCode)
        {
        case RISCV_LD:
        {
            core.drequest.datasize = core.extoMem.funct3.slc<2>(0);
            core.drequest.signenable = !core.extoMem.funct3.slc<1>(2);
#ifndef nocache
            core.drequest.address = core.extoMem.result;
            core.drequest.dcacheenable = true;
            core.drequest.writeenable = false;
            core.ctrl.cachelock = true;

            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;
            core.memtoWB.csr = false;
#else
            core.memtoWB.realInstruction = core.extoMem.realInstruction;
            simul(core.memtoWB.pc = core.extoMem.pc;
            core.memtoWB.instruction = core.extoMem.instruction;)
            core.memtoWB.rd = core.extoMem.rd;

            ac_int<32, false> mem_read = data_memory[core.extoMem.result >> 2];
            simul(cycles += MEMORY_READ_LATENCY;)
            formatread(core.extoMem.result, core.drequest.datasize, core.drequest.signenable, mem_read);
            core.memtoWB.result = mem_read;

            // data                                             size                @address
            coredebug("dR%d  @%06x   %08x   %08x   %s\n", core.drequest.datasize.to_int(), core.extoMem.result.to_int(),
                      data_memory[core.extoMem.result >> 2], mem_read.to_int(), core.drequest.signenable?"true":"false");
                   // what is in memory                  what is actually read    sign extension
#endif
            simul(if(core.extoMem.result >= 0x11040 && core.extoMem.result < 0x11048)
            {
                fprintf(stderr, "end here? %lld   @%06x   @%06x R%d\n", core.csrs.mcycle.to_int64(), core.extoMem.pc.to_int(), core.extoMem.result.to_int(),
                          core.drequest.datasize.to_int());
                core.ctrl.cachelock = false;
                core.memtoWB.result = 1;
            })
            break;
        }
        case RISCV_ST:
        {
            core.drequest.datasize = core.extoMem.funct3.slc<2>(0);
            core.drequest.signenable = core.extoMem.funct3.slc<1>(2);

            simul(if(core.extoMem.result >= 0x11000 && core.extoMem.result < 0x11040)
                  fprintf(stderr, "end here? %lld   @%06x   @%06x W%d  %08x\n", core.csrs.mcycle.to_int64(), core.extoMem.pc.to_int(), core.extoMem.result.to_int(),
                          core.drequest.datasize.to_int(), core.extoMem.datac.to_int());)
#ifndef nocache
            core.drequest.address = core.extoMem.result;
            core.drequest.dcacheenable = true;
            core.drequest.writeenable = true;
            core.drequest.writevalue = core.extoMem.datac;
            core.ctrl.cachelock = true;

            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;
            core.memtoWB.csr = false;
#else
            core.memtoWB.pc = 0;
            simul(core.memtoWB.instruction = 0;)
            core.memtoWB.rd = 0;
            core.memtoWB.realInstruction = false;

            ac_int<32, false> memory_val = data_memory[core.extoMem.result >> 2];
            simul(cycles += MEMORY_READ_LATENCY;)
            formatwrite(core.extoMem.result, core.drequest.datasize, memory_val, core.extoMem.datac);

            core.drequest.address = core.extoMem.result;
            core.drequest.writevalue = memory_val;   // dctrl is not used anyway

            // data                                         size                    @address
            coredebug("dW%d  @%06x   %08x   %08x   %08x\n", core.drequest.datasize.to_int(), core.extoMem.result.to_int(),
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
            core.memtoWB.realInstruction = core.extoMem.realInstruction;
            core.memtoWB.rd = core.extoMem.rd;

            core.memtoWB.csr = core.extoMem.csr;
            core.memtoWB.CSRid = core.extoMem.CSRid;
            core.memtoWB.rescsr = core.extoMem.datac;
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
    core.csrs.minstret += core.memtoWB.realInstruction;

#if !defined(COMET_NO_SYSTEM) && !defined(COMET_NO_CSR)
    if(core.memtoWB.csr)     // condition should be more precise
    {
        fprintf(stderr, "Writing %08x in CSR @%03x    @%06x\n", core.memtoWB.rescsr.to_int(), core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());

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
                        fprintf(stderr, "CSR write mstatus @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 1:
                        core.csrs.misa = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write misa @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 2:
                        core.csrs.medeleg = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write medeleg @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 3:
                        core.csrs.mideleg = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mideleg @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 4:
                        core.csrs.mie = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mie @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 5:
                        core.csrs.mtvec = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mtvec @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 6:
                        core.csrs.mcounteren = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mcounteren @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
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
                        fprintf(stderr, "CSR write mscratch @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 1:
                        core.csrs.mepc = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mepc @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 2:
                        core.csrs.mcause = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mcause @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 3:
                        core.csrs.mtval = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mtval @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                        break;
                    case 4:
                        core.csrs.mip = core.memtoWB.rescsr;
                        fprintf(stderr, "CSR write mip @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
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
                    core.csrs.mcycle.set_slc(0, core.memtoWB.rescsr);
                    fprintf(stderr, "CSR write mcycle low @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                    break;
                case 1:
                    core.csrs.minstret.set_slc(0, core.memtoWB.rescsr);
                    fprintf(stderr, "CSR write minstret low @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                    break;
                case 2:
                    core.csrs.mcycle.set_slc(32, core.memtoWB.rescsr);
                    fprintf(stderr, "CSR write mcycle high @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                    break;
                case 3:
                    core.csrs.minstret.set_slc(32, core.memtoWB.rescsr);
                    fprintf(stderr, "CSR write minstret high @%06x  %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.rescsr.to_int());
                    break;
                }
            }
            else if(core.memtoWB.CSRid.slc<2>(10) == 3)
            {
                fprintf(stderr, "Read only CSR %03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
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
#endif

    
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
            MultiCycleOp& mcop, MultiCycleRes mcres,
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
            MultiCycleOp& mcop, MultiCycleRes mcres,
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
