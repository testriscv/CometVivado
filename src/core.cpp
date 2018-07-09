/* vim: set ts=4 nu ai: */
#include "portability.h"
#include "riscvISA.h"
#include "core.h"
#include "cache.h"
#ifndef __SYNTHESIS__
#include "simulator.h"

#define EXDEFAULT() default: \
    fprintf(stderr, "Error : Unknown operation in Ex stage : @%06x	%08x\n", core.extoMem.pc.to_int(), core.extoMem.instruction.to_int()); \
    debug("Error : Unknown operation in Ex stage : @%06x	%08x\n", core.extoMem.pc.to_int(), core.extoMem.instruction.to_int()); \
    assert(false && "Unknown operation in Ex stage");

#else

#define EXDEFAULT()
#define fprintf(...)

#endif

const ac_int<32, false> CSR::mvendorid = 0;
const ac_int<32, false> CSR::marchid = 0;
const ac_int<32, false> CSR::mimpid = 0;

void memorySet(unsigned int memory[N], ac_int<32, false> address, ac_int<32, true> value, ac_int<2, false> op
            #ifndef __SYNTHESIS__
               , ac_int<64, false>& cycles
            #endif
               )
{
    ac_int<32, false> wrapped_address = address % (4*N);
    ac_int<32, false> location = wrapped_address >> 2;
    ac_int<32, false> memory_val = memory[location];
    simul(cycles += MEMORY_READ_LATENCY);
    formatwrite(address, op, memory_val, value);
    // data                                     size-1        @address         what was there before  what we want to write  what is actually written
    coredebug("dW%d  @%06x   %08x   %08x   %08x\n", op.to_int(), wrapped_address.to_int(), memory[location], value.to_int(), memory_val.to_int());
    memory[location] = memory_val;
}

ac_int<32, true> memoryGet(unsigned int memory[N], ac_int<32, false> address, ac_int<2, false> op, bool sign
                       #ifndef __SYNTHESIS__
                           , ac_int<64, false>& cycles
                       #endif
                           )
{
    ac_int<32, false> wrapped_address = address % (4*N);
    ac_int<32, false> location = wrapped_address >> 2;
    ac_int<32, false> mem_read = memory[location];
    simul(cycles += MEMORY_READ_LATENCY);
    formatread(address, op, sign, mem_read);
    // data                                   size-1        @address               what is in memory   what is actually read    sign extension
    coredebug("dR%d  @%06x   %08x   %08x   %s\n", op.to_int(), wrapped_address.to_int(), memory[location], mem_read.to_int(), sign?"true":"false");
    return mem_read;
}

void Ft(Core& core, unsigned int ins_memory[N]
    #ifndef __SYNTHESIS__
        , ac_int<64, false>& cycles
    #endif
        )
{
    ac_int<32, false> next_pc;
    if(core.freeze_fetch)
    {
        next_pc = core.pc;               // read after load dependency, stall 1 cycle
    }
    else
    {
        next_pc = core.pc + 4;
    }
    ac_int<32, false> jump_pc;
    if(core.mem_lock > 1)
    {
        jump_pc = next_pc;          // this means that we already had a jump before, so prevent double jumping when 2 jumps or branch are consecutive
    }
    else
    {
        jump_pc = core.extoMem.memValue; // forward the value from ex stage for jump & branch
    }
    bool control = 0;
    switch(core.extoMem.opCode)
    {
    case RISCV_BR:
        control = core.extoMem.result > 0 ? 1 : 0;   // result is the result of the comparison so either 1 or 0, memvalue is the jump address
        break;
    case RISCV_JAL:
        control = 1;
        break;
    case RISCV_JALR:
        control = 1;
        break;
    default:
        control = 0;
        break;
    }

#ifndef nocache
    if(!core.freeze_fetch)
    {
        if(core.insvalid && core.cachepc == core.pc)
        {
            core.ftoDC.instruction = core.instruction;
            core.ftoDC.pc = core.pc;
            if(control)
            {
                core.pc = jump_pc % (4*N);
            }
            else
            {
                core.pc = next_pc % (4*N);
            }
            debug("%06x\n", core.pc.to_int());
        }
        else
        {
            static bool init = false;
            core.ftoDC.instruction = 0x13;  // insert bubble (0x13 is NOP instruction and corresponds to ADDI r0, r0, 0)
            core.ftoDC.pc = 0;

            if(!init)
            {
                core.pc %= (4*N);
                init = true;
            }
            else if(core.mem_lock > 1)   // there was a jump ealier, so we do nothing because right instruction still hasn't been fetched
            {
                debug("Had a jump, not moving & ");
            }
            // if there's a jump at the same time of a miss, update to jump_pc
            else if(control)
            {
                core.pc = jump_pc % (4*N);
                debug("Jumping & ");
            }
            debug("Requesting @%06x\n", core.pc.to_int());
        }
        core.iaddress = core.pc;
    }
    else      // we cannot overwrite because it would not execute the frozen instruction
    {
        core.ftoDC.instruction = core.ftoDC.instruction;
        core.ftoDC.pc = core.ftoDC.pc;
        debug("Fetch frozen, what to do?");
    }

    simul(if(core.ftoDC.pc)
    {
        coredebug("Ft   @%06x   %08x\n", core.ftoDC.pc.to_int(), core.ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
#else
    if(!core.freeze_fetch)
    {
        core.ftoDC.instruction = ins_memory[core.pc/4];
        simul(cycles += MEMORY_READ_LATENCY);
        core.ftoDC.pc = core.pc;
        //debug("i @%06x (%06x)   %08x    S:3\n", core.pc.to_int(), core.pc.to_int()/4, ins_memory[core.pc/4]);
    }
    else      // we cannot overwrite because it would not execute the frozen instruction
    {
        core.ftoDC.instruction = core.ftoDC.instruction;
        core.ftoDC.pc = core.ftoDC.pc;
        debug("Fetch frozen, what to do?");
    }
    simul(if(core.ftoDC.pc)
    {
        coredebug("Ft   @%06x   %08x\n", core.ftoDC.pc.to_int(), core.ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
    core.pc = control ? jump_pc : next_pc;
#endif
}

void DC(Core& core)
{
    ac_int<32, false> pc = core.ftoDC.pc;
    ac_int<32, false> instruction = core.ftoDC.instruction;

    // R-type instruction
    ac_int<7, false> funct7 = instruction.slc<7>(25);
    ac_int<5, false> rs2 = instruction.slc<5>(20);
    ac_int<5, false> rs1 = instruction.slc<5>(15);
    ac_int<7, false> funct3 = instruction.slc<3>(12);
    ac_int<5, false> rd = instruction.slc<5>(7);
    ac_int<5, false> opCode = instruction.slc<5>(2);    // reduced to 5 bits because 1:0 is always 11

    assert(instruction.slc<2>(0) == 3 && "Instruction lower bits are not 0b11, illegal instruction");

    // share immediate for all type of instruction
    // this should simplify the hardware
    // double cast as signed int before equality for sign extension
    ac_int<32, true> immediate = (ac_int<32, true>)(((ac_int<32, true>)instruction).slc<1>(31));

    coredebug("%08x \n", immediate.to_int());

    // S-type instruction
    // use rs2 rs1 funct3 opCode from R-type
    // 12 bits immediate
    if(opCode == RISCV_ST)
    {
        immediate.set_slc(5, instruction.slc<6>(25));
        immediate.set_slc(0, instruction.slc<5>(7));
    }
    // B-type instruction
    // use rs2 rs1 funct3 opCode from R-type
    // 13 bits immediate
    else if(opCode == RISCV_BR)
    {
        immediate.set_slc(0, (ac_int<1, true>)0);
        immediate.set_slc(1, instruction.slc<4>(8));
        immediate.set_slc(5, instruction.slc<6>(25));
        immediate.set_slc(11, instruction.slc<1>(7));
    }
    // J-type instruction
    // use rd opCode from R-type
    // 20 bits immediate
    else if(opCode == RISCV_JAL)
    {
        immediate.set_slc(0, (ac_int<1, true>)0);
        immediate.set_slc(1, instruction.slc<4>(21));
        immediate.set_slc(5, instruction.slc<6>(25));
        immediate.set_slc(11, instruction.slc<1>(20));
        immediate.set_slc(12, instruction.slc<8>(12));
    }
    // U-type instruction
    // use rd opCode from R-type
    // 20 bits immediate
    else if(opCode == RISCV_LUI || opCode == RISCV_AUIPC)
    {
        immediate.set_slc(0, (ac_int<12, true>)0);
        immediate.set_slc(12, instruction.slc<19>(12));
    }
    // I-type instruction
    // use rs1 funct3 rd opCode from R-type
    // 12 bits immediate
    else
    {
        immediate.set_slc(0, instruction.slc<11>(20));
    }


    ac_int<6, false> shamt = core.ftoDC.instruction.slc<5>(20);
    ac_int<12, false> imm12_I = core.ftoDC.instruction.slc<12>(20);
    bool forward_rs1 = false;
    bool forward_rs2 = false;
    bool forward_ex_or_mem_rs1 = false;
    bool forward_ex_or_mem_rs2 = false;
    bool datab_fwd = 0;
    ac_int<12, true> store_imm = 0;
    store_imm.set_slc(0,core.ftoDC.instruction.slc<5>(7));
    store_imm.set_slc(5,core.ftoDC.instruction.slc<7>(25));

    core.dctoEx.opCode = opCode;
    core.dctoEx.funct3 = funct3;
    core.dctoEx.funct7 = funct7;
    core.dctoEx.shamt = shamt;
    core.dctoEx.rs1 = rs1;
    core.dctoEx.rs2 = 0;
    core.dctoEx.pc = core.ftoDC.pc;
    core.dctoEx.instruction = core.ftoDC.instruction;
    core.freeze_fetch = 0;
    switch (opCode)
    {
    case RISCV_LUI:
        core.dctoEx.dest = rd;
        core.dctoEx.datab = immediate;
        break;
    case RISCV_AUIPC:
        core.dctoEx.dest = rd;
        core.dctoEx.datab = immediate;
        break;
    case RISCV_JAL:
        core.dctoEx.dest = rd;
        core.dctoEx.datab = immediate;
        break;
    case RISCV_JALR:
        core.dctoEx.dest = rd;
        core.dctoEx.datab = immediate;
        break;
    case RISCV_BR:
        core.dctoEx.rs2 = rs2;
        datab_fwd = 1;
        core.dctoEx.datac = immediate;
        core.dctoEx.dest = 0;
        break;
    case RISCV_LD:
        core.dctoEx.dest = rd;
        core.dctoEx.memValue = immediate;
        break;
    case RISCV_ST:
        core.dctoEx.datad = rs2;
        core.dctoEx.memValue = store_imm;
        core.dctoEx.dest = 0;
        break;
    case RISCV_OPI:
        core.dctoEx.dest = rd;
        core.dctoEx.datab = immediate;
        break;
    case RISCV_OP:
        core.dctoEx.rs2 = rs2;
        datab_fwd = 1;
        core.dctoEx.dest = rd;
        break;
    case RISCV_SYSTEM:
        if(core.dctoEx.funct3 == RISCV_SYSTEM_ENV)
        {
            datab_fwd = 1;
            core.dctoEx.dest = 10;
            rs1 = 17;
            rs2 = 10;
            core.dctoEx.datac = (core.extoMem.dest == 11 && core.mem_lock < 2) ? core.extoMem.result : ((core.memtoWB.dest == 11 && core.mem_lock == 0) ? core.memtoWB.result : core.REG[11]);
            core.dctoEx.datad = (core.extoMem.dest == 12 && core.mem_lock < 2) ? core.extoMem.result : ((core.memtoWB.dest == 12 && core.mem_lock == 0) ? core.memtoWB.result : core.REG[12]);
            core.dctoEx.datae = (core.extoMem.dest == 13 && core.mem_lock < 2) ? core.extoMem.result : ((core.memtoWB.dest == 13 && core.mem_lock == 0) ? core.memtoWB.result : core.REG[13]);
        }
        else simul(if(core.dctoEx.funct3 != 0x4))
        {
            // handle the case for rd = 0 for CSRRW
            // handle WIRI/WARL/etc.
            if(imm12_I.slc<2>(8) == 3)
            {
                if(imm12_I.slc<2>(10) == 0)
                {
                    if(imm12_I.slc<1>(6) == 0)  // 0x30X
                    {
                        switch(imm12_I.slc<3>(0))
                        {
                        case 0:
                            core.dctoEx.datab = core.csrs.mstatus;
                            break;
                        case 1:
                            core.dctoEx.datab = core.csrs.misa;
                            break;
                        case 2:
                            core.dctoEx.datab = core.csrs.medeleg;
                            break;
                        case 3:
                            core.dctoEx.datab = core.csrs.mideleg;
                            break;
                        case 4:
                            core.dctoEx.datab = core.csrs.mie;
                            break;
                        case 5:
                            core.dctoEx.datab = core.csrs.mtvec;
                            break;
                        case 6:
                            core.dctoEx.datab = core.csrs.mcounteren;
                            break;
                        default:
                            fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", imm12_I.to_int(), core.dctoEx.pc.to_int());
                            assert(false && "Unknown CSR id\n");
                            break;
                        }
                    }
                    else                        // 0x34X
                    {
                        switch(imm12_I.slc<3>(0))
                        {
                        case 0:
                            core.dctoEx.datab = core.csrs.mscratch;
                            break;
                        case 1:
                            core.dctoEx.datab = core.csrs.mepc;
                            break;
                        case 2:
                            core.dctoEx.datab = core.csrs.mcause;
                            break;
                        case 3:
                            core.dctoEx.datab = core.csrs.mtval;
                            break;
                        case 4:
                            core.dctoEx.datab = core.csrs.mip;
                            break;
                        default:
                            fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", imm12_I.to_int(), core.dctoEx.pc.to_int());
                            assert(false && "Unknown CSR id\n");
                            break;
                        }
                    }
                }
                else if(imm12_I.slc<2>(10) == 2)
                {
                    ac_int<2, false> foo = 0;
                    foo.set_slc(0, imm12_I.slc<1>(1));
                    foo.set_slc(1, imm12_I.slc<1>(7));
                    switch(foo)
                    {
                    case 0:
                        core.dctoEx.datab = core.csrs.mcycle.slc<32>(0);
                        break;
                    case 1:
                        core.dctoEx.datab = core.csrs.minstret.slc<32>(0);
                        break;
                    case 2:
                        core.dctoEx.datab = core.csrs.mcycle.slc<32>(32);
                        break;
                    case 3:
                        core.dctoEx.datab = core.csrs.minstret.slc<32>(32);
                        break;
                    }
                }
                else if(imm12_I.slc<2>(10) == 3)
                {
                    switch(imm12_I.slc<2>(0))
                    {
                    case 1:
                        core.dctoEx.datab = core.csrs.mvendorid;
                        break;
                    case 2:
                        core.dctoEx.datab = core.csrs.marchid;
                        break;
                    case 3:
                        core.dctoEx.datab = core.csrs.mimpid;
                        break;
                    case 0:
                        core.dctoEx.datab = core.csrs.mhartid;
                        break;
                    }
                }
                else
                {
                    fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", imm12_I.to_int(), core.dctoEx.pc.to_int());
                    assert(false && "Unknown CSR id\n");
                }
            }
            else
            {
                fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", imm12_I.to_int(), core.dctoEx.pc.to_int());
                assert(false && "Unknown CSR id\n");
            }
            fprintf(stderr, "Reading %08x in CSR @%03x    @%06x\n", core.dctoEx.datab.to_int(), imm12_I.to_int(), core.dctoEx.pc.to_int());
            core.dctoEx.memValue = imm12_I;
            core.dctoEx.dest = rd;
            //dataa will contain core.REG[rs1]
        }
        simul(else assert(false));
        break;
    }

    ac_int<32, true> REG_rs1 = core.REG[rs1.slc<5>(0)];
    ac_int<32, true> REG_rs2 = core.REG[rs2.slc<5>(0)];

    forward_rs1 = ((core.extoMem.dest == rs1 && core.mem_lock < 2) || (core.memtoWB.dest == rs1 && core.mem_lock == 0)) ? 1 : 0;
    forward_rs2 = ((core.extoMem.dest == rs2 && core.mem_lock < 2) || (core.memtoWB.dest == rs2 && core.mem_lock == 0)) ? 1 : 0;
    forward_ex_or_mem_rs1 = (core.extoMem.dest == rs1) ? 1 : 0;
    forward_ex_or_mem_rs2 = (core.extoMem.dest == rs2) ? 1 : 0;

    core.dctoEx.dataa = (forward_rs1 && rs1 != 0) ? (forward_ex_or_mem_rs1 ? core.extoMem.result : core.memtoWB.result) : REG_rs1;

    if(opCode == RISCV_ST)
    {
        core.dctoEx.datac = (forward_rs2 && rs2 != 0) ? (forward_ex_or_mem_rs2 ? core.extoMem.result : core.memtoWB.result) : REG_rs2;
    }
    if(datab_fwd)
    {
        core.dctoEx.datab = (forward_rs2 && rs2 != 0) ? (forward_ex_or_mem_rs2 ? core.extoMem.result : core.memtoWB.result) : REG_rs2;
    }
    // stall 1 cycle if load to one of the core.REGister we use, e.g lw a5, someaddress followed by any operation that use a5
    if(core.prev_opCode == RISCV_LD && (core.extoMem.dest == rs1 || (opCode != RISCV_LD && core.extoMem.dest == rs2)) && core.mem_lock < 2 && core.prev_pc != core.ftoDC.pc)
    {
        core.freeze_fetch = 1;
        core.ex_bubble = 1;
    }
    // add a stall for core.csrs instruction because they should be atomic? Detect if 2 core.csrs follow each other
    core.prev_opCode = opCode;
    core.prev_pc = core.ftoDC.pc;

    simul(if(core.dctoEx.pc)
    {
        coredebug("Dc   @%06x   %08x\n", core.dctoEx.pc.to_int(), core.dctoEx.instruction.to_int());
    }
    else
    {
        coredebug("Dc   \n");
    })
}

void Ex(Core& core
    #ifndef __SYNTHESIS__
        , Simulator* sim
    #endif
        )
{
    ac_int<32, false> unsignedReg1 = 0;
    unsignedReg1.set_slc(0,(core.dctoEx.dataa).slc<32>(0));
    ac_int<32, false> unsignedReg2 = 0;
    unsignedReg2.set_slc(0,(core.dctoEx.datab).slc<32>(0));
    ac_int<33, true> mul_reg_a = 0;
    ac_int<33, true> mul_reg_b = 0;
    ac_int<66, true> longResult = 0;
    core.extoMem.pc = core.dctoEx.pc;
    core.extoMem.instruction = core.dctoEx.instruction;
    core.extoMem.opCode = core.dctoEx.opCode;
    core.extoMem.dest = core.dctoEx.dest;
    core.extoMem.datac = core.dctoEx.datac;
    core.extoMem.funct3 = core.dctoEx.funct3;
    core.extoMem.datad = core.dctoEx.datad;
    core.extoMem.sys_status = 0;
    if ((core.extoMem.opCode != RISCV_BR) && (core.extoMem.opCode != RISCV_ST) && (core.extoMem.opCode != RISCV_MISC_MEM))
    {
        core.extoMem.WBena = 1;
    }
    else
    {
        core.extoMem.WBena = 0;
    }

    switch (core.dctoEx.opCode)
    {
    case RISCV_LUI:
        core.extoMem.result = core.dctoEx.datab;
        break;
    case RISCV_AUIPC:
        core.extoMem.result = core.dctoEx.pc + core.dctoEx.datab;
        break;
    case RISCV_JAL:
        core.extoMem.result = core.dctoEx.pc + 4;
        core.extoMem.memValue = core.dctoEx.pc + core.dctoEx.datab;
        break;
    case RISCV_JALR:
        core.extoMem.result = core.dctoEx.pc + 4;
        core.extoMem.memValue = (core.dctoEx.dataa + core.dctoEx.datab) & 0xfffffffe;
        break;
    case RISCV_BR: // Switch case for branch instructions
        switch(core.dctoEx.funct3)
        {
        case RISCV_BR_BEQ:
            core.extoMem.result = (core.dctoEx.dataa == core.dctoEx.datab);
            break;
        case RISCV_BR_BNE:
            core.extoMem.result = (core.dctoEx.dataa != core.dctoEx.datab);
            break;
        case RISCV_BR_BLT:
            core.extoMem.result = (core.dctoEx.dataa < core.dctoEx.datab);
            break;
        case RISCV_BR_BGE:
            core.extoMem.result = (core.dctoEx.dataa >= core.dctoEx.datab);
            break;
        case RISCV_BR_BLTU:
            core.extoMem.result = (unsignedReg1 < unsignedReg2);
            break;
        case RISCV_BR_BGEU:
            core.extoMem.result = (unsignedReg1 >= unsignedReg2);
            break;
        EXDEFAULT();
        }
        core.extoMem.memValue = core.dctoEx.pc + core.dctoEx.datac;
        break;
    case RISCV_LD:
        core.extoMem.result = (core.dctoEx.dataa + core.dctoEx.memValue);
        break;
    case RISCV_ST:
        core.extoMem.result = (core.dctoEx.dataa + core.dctoEx.memValue);
        core.extoMem.datac = core.dctoEx.datac;
        break;
    case RISCV_OPI:
        switch(core.dctoEx.funct3)
        {
        case RISCV_OPI_ADDI:
            core.extoMem.result = core.dctoEx.dataa + core.dctoEx.datab;
            break;
        case RISCV_OPI_SLTI:
            core.extoMem.result = (core.dctoEx.dataa < core.dctoEx.datab) ? 1 : 0;
            break;
        case RISCV_OPI_SLTIU:
            core.extoMem.result = (unsignedReg1 < ((ac_int<32, false>)core.dctoEx.datab)) ? 1 : 0;
            break;
        case RISCV_OPI_XORI:
            core.extoMem.result = core.dctoEx.dataa ^ core.dctoEx.datab;
            break;
        case RISCV_OPI_ORI:
            core.extoMem.result =  core.dctoEx.dataa | core.dctoEx.datab;
            break;
        case RISCV_OPI_ANDI:
            core.extoMem.result = core.dctoEx.dataa & core.dctoEx.datab;
            break;
        case RISCV_OPI_SLLI:
            core.extoMem.result = core.dctoEx.dataa << core.dctoEx.shamt;
            break;
        case RISCV_OPI_SRI:
            if (core.dctoEx.funct7.slc<1>(5))    //SRAI
                core.extoMem.result = core.dctoEx.dataa >> core.dctoEx.shamt;
            else                            //SRLI
                core.extoMem.result = ((ac_int<32, false>)core.dctoEx.dataa) >> core.dctoEx.shamt;
            //fprintf(stderr, "@%06x      %08x >> %02x = %08x\n", core.dctoEx.pc.to_int(), core.dctoEx.dataa.to_int(), core.dctoEx.shamt.to_int(), core.extoMem.result.to_int());
            break;
        EXDEFAULT();
        }
        break;
    case RISCV_OP:
        if(core.dctoEx.funct7.slc<1>(0))     // M Extension
        {
            mul_reg_a = core.dctoEx.dataa;
            mul_reg_b = core.dctoEx.datab;
            mul_reg_a[32] = core.dctoEx.dataa[31];
            mul_reg_b[32] = core.dctoEx.datab[31];
            switch (core.dctoEx.funct3)  //Switch case for multiplication operations (RV32M)
            {
            case RISCV_OP_M_MULHSU:
                mul_reg_b[32] = 0;
                break;
            case RISCV_OP_M_MULHU:
                mul_reg_a[32] = 0;
                mul_reg_b[32] = 0;
                break;
        #ifndef __SYNTHESIS__
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
        #ifndef __SYNTHESIS__
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
                //fprintf(stderr, "DIV @%06x : %08x / %08x = %08x\n", core.dctoEx.pc.to_int(), mul_reg_a.to_int(), mul_reg_b.to_int(), longResult.to_int());
                break;
            case RISCV_OP_M_REM:
            case RISCV_OP_M_REMU:
                if(mul_reg_b)
                    longResult = mul_reg_a % mul_reg_b;
                else    // modulo 0 => result is first operand
                    longResult = mul_reg_a;
                //fprintf(stderr, "REM @%06x : %08x %% %08x = %08x\n", core.dctoEx.pc.to_int(), mul_reg_a.to_int(), mul_reg_b.to_int(), longResult.to_int());
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
                    core.extoMem.result = core.dctoEx.dataa - core.dctoEx.datab;
                else                            // ADD
                    core.extoMem.result = core.dctoEx.dataa + core.dctoEx.datab;
                break;
            case RISCV_OP_SLL:
                core.extoMem.result = core.dctoEx.dataa << ((ac_int<5, false>)core.dctoEx.datab);
                break;
            case RISCV_OP_SLT:
                core.extoMem.result = (core.dctoEx.dataa < core.dctoEx.datab) ? 1 : 0;
                break;
            case RISCV_OP_SLTU:
                core.extoMem.result = (unsignedReg1 < unsignedReg2) ? 1 : 0;
                break;
            case RISCV_OP_XOR:
                core.extoMem.result = core.dctoEx.dataa ^ core.dctoEx.datab;
                break;
            case RISCV_OP_SR:
                if(core.dctoEx.funct7.slc<1>(5))     // SRA
                    core.extoMem.result = core.dctoEx.dataa >> ((ac_int<5, false>)core.dctoEx.datab);
                else                            // SRL
                    core.extoMem.result = ((ac_int<32, false>)core.dctoEx.dataa) >> ((ac_int<5, false>)core.dctoEx.datab);
                break;
            case RISCV_OP_OR:
                core.extoMem.result = core.dctoEx.dataa | core.dctoEx.datab;
                break;
            case RISCV_OP_AND:
                core.extoMem.result =  core.dctoEx.dataa & core.dctoEx.datab;
                break;
            EXDEFAULT();
            }
        }
        break;
    case RISCV_MISC_MEM:    // this does nothing because all memory accesses are ordered and we have only one core
        break;
    case RISCV_SYSTEM:
        if(core.dctoEx.funct3 == 0)
        {
        #ifndef __SYNTHESIS__
            core.extoMem.result = sim->solveSyscall(core.dctoEx.dataa, core.dctoEx.datab, core.dctoEx.datac, core.dctoEx.datad, core.dctoEx.datae, core.extoMem.sys_status);
            fprintf(stderr, "Syscall @%06x\n", core.dctoEx.pc.to_int());
        #endif
        }
        else
        {
            switch(core.dctoEx.funct3)   // dataa is from rs1, datab is from csr
            {   // case 0: mret instruction, core.dctoEx.memValue should be 0x302
            case RISCV_SYSTEM_CSRRW:
                core.extoMem.datac = core.dctoEx.dataa;       // written back to csr
                fprintf(stderr, "CSRRW @%03x    @%06x\n", core.dctoEx.memValue.to_int(), core.dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRS:
                core.extoMem.datac = core.dctoEx.dataa | core.dctoEx.datab;
                fprintf(stderr, "CSRRS @%03x    @%06x\n", core.dctoEx.memValue.to_int(), core.dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRC:
                core.extoMem.datac = ((ac_int<32, false>)~core.dctoEx.dataa) & core.dctoEx.datab;
                fprintf(stderr, "CSRRC @%03x    @%06x\n", core.dctoEx.memValue.to_int(), core.dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRWI:
                core.extoMem.datac = core.dctoEx.rs1;
                fprintf(stderr, "CSRRWI @%03x    @%06x\n", core.dctoEx.memValue.to_int(), core.dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRSI:
                core.extoMem.datac = core.dctoEx.rs1 | core.dctoEx.datab;
                fprintf(stderr, "CSRRSI @%03x    @%06x\n", core.dctoEx.memValue.to_int(), core.dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRCI:
                core.extoMem.datac = ((ac_int<32, false>)~core.dctoEx.rs1) & core.dctoEx.datab;
                fprintf(stderr, "CSRRCI @%03x    @%06x\n", core.dctoEx.memValue.to_int(), core.dctoEx.pc.to_int());
                break;
            EXDEFAULT();
            }
            core.extoMem.result = core.dctoEx.datab;      // written back to rd
            core.extoMem.memValue = core.dctoEx.memValue;
            core.extoMem.rs1 = core.dctoEx.rs1;
        }
        break;
    EXDEFAULT();
    }

    if(core.ex_bubble)
    {
        core.mem_bubble = 1;
        core.extoMem.pc = 0;
        core.extoMem.result = 0; //Result of the EX stage
        core.extoMem.datad = 0;
        core.extoMem.datac = 0;
        core.extoMem.dest = 0;
        core.extoMem.WBena = 0;
        core.extoMem.opCode = 0;
        core.extoMem.memValue = 0;
        core.extoMem.rs1 = 0;
        core.extoMem.funct3 = 0;
    }
    core.ex_bubble = 0;

    simul(if(core.extoMem.pc)
    {
        coredebug("Ex   @%06x   %08x\n", core.extoMem.pc.to_int(), core.extoMem.instruction.to_int());
    }
    else
    {
        coredebug("Ex   \n");
    })

}

void do_Mem(Core& core, unsigned int data_memory[N]
            #ifndef __SYNTHESIS__
                , ac_int<64, false>& cycles
            #endif
            )          // data & acknowledgment from cache
{
    if(core.cachelock)
    {
        if(core.datavalid)
        {
            core.memtoWB.WBena = core.extoMem.WBena;
            core.memtoWB.pc = core.extoMem.pc;
            if(!core.writeenable)
                core.memtoWB.result = core.readvalue;
            core.cachelock = false;
            core.dcacheenable = false;
            core.writeenable = false;
        }
        else
        {
            core.memtoWB.pc = 0;
        }
    }
    else if(core.mem_bubble)
    {
        core.mem_bubble = 0;
        //wb_bubble = 1;
        core.memtoWB.pc = 0;
        core.memtoWB.instruction = 0;
        core.memtoWB.result = 0; //Result to be written back
        core.memtoWB.dest = 0; //Register to be written at WB stage
        core.memtoWB.WBena = 0; //Is a WB is needed ?
        core.memtoWB.opCode = 0;
        core.memtoWB.sys_status = 0;
    }
    else
    {
        if(core.mem_lock > 0)
        {
            core.mem_lock = core.mem_lock - 1;
            core.memtoWB.WBena = 0;
            simul(if(core.mem_lock)
                debug("I    @%06x\n", core.extoMem.pc.to_int());
            )
        }

        core.memtoWB.sys_status = core.extoMem.sys_status;
        core.memtoWB.opCode = core.extoMem.opCode;
        core.memtoWB.result = core.extoMem.result;
        core.memtoWB.CSRid = core.extoMem.memValue;
        core.memtoWB.rescsr = core.extoMem.datac;
        core.memtoWB.rs1 = core.extoMem.rs1;
        core.memtoWB.csrwb = false;

        bool sign = 0;

        if(core.mem_lock == 0)
        {
            core.memtoWB.pc = core.extoMem.pc;
            core.memtoWB.instruction = core.extoMem.instruction;
            core.memtoWB.WBena = core.extoMem.WBena;
            core.memtoWB.dest = core.extoMem.dest;
            switch(core.extoMem.opCode)
            {
            case RISCV_BR:
                if (core.extoMem.result)
                {
                    core.mem_lock = 3;
                }
                core.memtoWB.WBena = 0;
                core.memtoWB.dest = 0;
                break;
            case RISCV_JAL:
                core.mem_lock = 3;
                break;
            case RISCV_JALR:
                core.mem_lock = 3;
                break;
            case RISCV_LD:
                switch(core.extoMem.funct3)
                {
                case RISCV_LD_LW:
                    core.datasize = 3;
                    sign = 1;
                    break;
                case RISCV_LD_LH:
                    core.datasize = 1;
                    sign = 1;
                    break;
                case RISCV_LD_LHU:
                    core.datasize = 1;
                    sign = 0;
                    break;
                case RISCV_LD_LB:
                    core.datasize = 0;
                    sign = 1;
                    break;
                case RISCV_LD_LBU:
                    core.datasize = 0;
                    sign = 0;
                    break;
                }
#ifndef nocache
                core.daddress = core.extoMem.result % (4*N);
                core.signenable = sign;
                core.dcacheenable = true;
                core.writeenable = false;
                core.cachelock = true;
                core.memtoWB.WBena = false;
                core.memtoWB.pc = 0;
#else
                //debug("%5d  ", cycles);
                core.memtoWB.result = memoryGet(data_memory, core.extoMem.result, core.datasize, sign
                                       #ifndef __SYNTHESIS__
                                           , cycles
                                       #endif
                                           );
#endif
                break;
            case RISCV_ST:
                switch(core.extoMem.funct3)
                {
                case RISCV_ST_STW:
                    core.datasize = 3;
                    break;
                case RISCV_ST_STH:
                    core.datasize = 1;
                    break;
                case RISCV_ST_STB:
                    core.datasize = 0;
                    break;
                }
#ifndef nocache
                core.daddress = core.extoMem.result % (4*N);
                core.signenable = false;
                core.dcacheenable = true;
                core.writeenable = true;
                core.writevalue = core.extoMem.datac;
                core.cachelock = true;
                core.memtoWB.WBena = false;
                core.memtoWB.pc = 0;
#else
                //debug("%5d  ", cycles);
                memorySet(data_memory, core.extoMem.result, core.extoMem.datac, core.datasize
                      #ifndef __SYNTHESIS__
                          , cycles
                      #endif
                          );
#endif
                break;
            case RISCV_SYSTEM:
                core.memtoWB.csrwb = true;
                break;
            }
        }
    }
    simul(if(core.memtoWB.pc)
    {
        coredebug("Mem  @%06x   %08x\n", core.memtoWB.pc.to_int(), core.memtoWB.instruction.to_int());
    }
    else
    {
        coredebug("Mem  \n");
    })
}

void doWB(Core& core)
{
    if (core.memtoWB.WBena == 1 && core.memtoWB.dest != 0)
    {
        core.REG[core.memtoWB.dest.slc<5>(0)] = core.memtoWB.result;
    }

    if(core.memtoWB.rs1 && core.memtoWB.csrwb)     // condition should be more precise
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
                        fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
                        assert(false && "Unknown CSR id\n");
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
                        fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
                        assert(false && "Unknown CSR id\n");
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
                fprintf(stderr, "Read only CSR %03x", core.memtoWB.CSRid);
                assert(false && "Read only CSR\n");
            }
            else
            {
                fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
                assert(false && "Unknown CSR id\n");
            }
        }
        else
        {
            fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", core.memtoWB.CSRid.to_int(), core.memtoWB.pc.to_int());
            assert(false && "Unknown CSR id\n");
        }
    }

#ifndef __SYNTHESIS__
    if(core.memtoWB.sys_status == 1)
    {
        debug("\nExit system call received, Exiting...");
        core.early_exit = 1;
    }
    else if(core.memtoWB.sys_status == 2)
    {
        debug("\nUnknown system call received, Exiting...");
        core.early_exit = 1;
    }
#endif

    if(core.memtoWB.pc)
    {
        if(core.memtoWB.pc != core.memtoWB.lastpc)
        {
            core.csrs.minstret += 1;
            core.memtoWB.lastpc = core.memtoWB.pc;
        }
        coredebug("\nWB   @%06x   %08x   (%d)\n", core.memtoWB.pc.to_int(), core.memtoWB.instruction.to_int(), core.csrs.minstret.to_int64());
    }
    else
    {
        coredebug("\nWB   \n");
    }

}

void coreinit(Core& core, ac_int<32, false> startpc)
{
    core.init = true;
    core.pc = startpc;   // startpc - 4 ?

    core.ftoDC.instruction = core.dctoEx.instruction =
    core.extoMem.instruction = core.memtoWB.instruction = 0x13;
    core.dctoEx.opCode = core.extoMem.opCode = core.memtoWB.opCode = RISCV_OPI;

    core.REG[2] = STACK_INIT;

#if Policy == RANDOM
    core.dctrl.policy = 0xF2D4B698;
    core.ictrl.policy = 0xF2D4B698;
#endif
}

void doStep(ac_int<32, false> startpc, unsigned int ins_memory[N], unsigned int dm[N], bool& exit
        #ifndef __SYNTHESIS__
            , ac_int<64, false>& c, ac_int<64, false>& numins, Simulator* sim
        #endif
            )
{
    static Core core = {0};

#ifdef __SYNTHESIS__
    static bool idummy = ac::init_array<AC_VAL_DC>((unsigned int*)core.idata, Sets*Associativity*Blocksize);
    (void)idummy;
    static bool itaginit = ac::init_array<AC_VAL_DC>((ac_int<32-tagshift, false>*)core.ictrl.tag, Sets*Associativity);
    (void)itaginit;
    static bool ivalinit = ac::init_array<AC_VAL_0>((bool*)core.ictrl.valid, Sets*Associativity);
    (void)ivalinit;

    static bool dummy = ac::init_array<AC_VAL_DC>((unsigned int*)core.ddata, Sets*Associativity*Blocksize);
    (void)dummy;
    static bool taginit = ac::init_array<AC_VAL_DC>((ac_int<32-tagshift, false>*)core.dctrl.tag, Sets*Associativity);
    (void)taginit;
    static bool dirinit = ac::init_array<AC_VAL_DC>((bool*)core.dctrl.dirty, Sets*Associativity);
    (void)dirinit;
    static bool valinit = ac::init_array<AC_VAL_0>((bool*)core.dctrl.valid, Sets*Associativity);
    (void)valinit;
#if Policy == FIFO
    static bool dpolinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)core.dctrl.policy, Sets);
    (void)dpolinit;
    static bool ipolinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)core.ictrl.policy, Sets);
    (void)ipolinit;
#elif Policy == LRU
    static bool dpolinit = ac::init_array<AC_VAL_DC>((ac_int<Associativity * (Associativity-1) / 2, false>*)core.dctrl.policy, Sets);
    (void)dpolinit;
    static bool ipolinit = ac::init_array<AC_VAL_DC>((ac_int<Associativity * (Associativity-1) / 2, false>*)core.ictrl.policy, Sets);
    (void)ipolinit;
#endif
#endif  // __SYNTHESIS__

    if(!core.init)
    {
        coreinit(core, startpc);

        simul(sim->setCore(core.REG, &core.dctrl, core.ddata));
    }


    simul(uint64_t oldcycles = core.csrs.mcycle);
    core.csrs.mcycle += 1;

    doWB(core);
    simul(coredebug("%d ", core.csrs.mcycle.to_int64());
    for(int i=0; i<32; i++)
    {
        if(core.REG[i])
            coredebug("%d:%08x ", i, (int)core.REG[i]);
    }
    )
    coredebug("\n");

    do_Mem(core , dm                                                     // data & acknowledgment from cache
       #ifndef __SYNTHESIS__
           , core.csrs.mcycle
       #endif
           );

#ifndef nocache
    dcache(core.dctrl, dm, core.ddata, core.daddress, core.datasize, core.signenable, core.dcacheenable,
           core.writeenable, core.writevalue, core.readvalue, core.datavalid
       #ifndef __SYNTHESIS__
           , core.csrs.mcycle
       #endif
           );
#endif

    if(!core.cachelock)
    {
        Ex(core
   #ifndef __SYNTHESIS__
           , sim
   #endif
           );
        DC(core);
        Ft(core, ins_memory
   #ifndef __SYNTHESIS__
          , core.csrs.mcycle
   #endif
          );
    }

    // icache is outside because it can still continues fetching
#ifndef nocache
    icache(core.ictrl, ins_memory, core.idata, core.iaddress, core.cachepc, core.instruction, core.insvalid
       #ifndef __SYNTHESIS__
           , core.csrs.mcycle
       #endif
           );
#endif

    simul(
    int M = MEMORY_READ_LATENCY>MEMORY_WRITE_LATENCY?MEMORY_READ_LATENCY:MEMORY_WRITE_LATENCY;
    if(oldcycles + M < core.csrs.mcycle)
    {
        core.csrs.mcycle = oldcycles + M; // we cannot step slower than the worst latency
    }
    c = core.csrs.mcycle;
    numins = core.csrs.minstret;
    )


    // riscv specification v2.2 p11
    assert((core.pc.to_int() & 3) == 0 && "An instruction address misaligned exception is generated on a taken branch or unconditional jump if the target address is not four-byte aligned.");

    simul(
    if(core.early_exit)
    {
        exit = true;
    }
    #ifndef nocache
        //cache write back for simulation
        for(unsigned int i  = 0; i < Sets; ++i)
            for(unsigned int j = 0; j < Associativity; ++j)
                if(core.dctrl.dirty[i][j] && core.dctrl.valid[i][j])
                    for(unsigned int k = 0; k < Blocksize; ++k)
                        dm[(core.dctrl.tag[i][j].to_int() << (tagshift-2)) | (i << (setshift-2)) | k] = core.ddata[i][k][j];
    #endif
    )
}
