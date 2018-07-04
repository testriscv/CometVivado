/* vim: set ts=4 nu ai: */
#include "riscvISA.h"
#include "core.h"
#include "cache.h"
#ifndef __SYNTHESIS__
#include "simulator.h"

#define EXDEFAULT() default: \
    fprintf(stderr, "Error : Unknown operation in Ex stage : @%06x	%08x\n", extoMem.pc.to_int(), extoMem.instruction.to_int()); \
    debug("Error : Unknown operation in Ex stage : @%06x	%08x\n", extoMem.pc.to_int(), extoMem.instruction.to_int()); \
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
    ac_int<32, false> wrapped_address = address % N;
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
    ac_int<32, false> wrapped_address = address % N;
    ac_int<32, false> location = wrapped_address >> 2;
    ac_int<32, false> mem_read = memory[location];
    simul(cycles += MEMORY_READ_LATENCY);
    formatread(address, op, sign, mem_read);
    // data                                   size-1        @address               what is in memory   what is actually read    sign extension
    coredebug("dR%d  @%06x   %08x   %08x   %s\n", op.to_int(), wrapped_address.to_int(), memory[location], mem_read.to_int(), sign?"true":"false");
    return mem_read;
}

void Ft(ac_int<32, false>& pc, bool freeze_fetch, ExtoMem extoMem, FtoDC& ftoDC, ac_int<3, false> mem_lock, // cpu internal logic
        ac_int<32, false>& iaddress,                                                // to cache
        ac_int<32, false> cachepc, unsigned int instruction, bool insvalid,         // from cache
        unsigned int ins_memory[N]
    #ifndef __SYNTHESIS__
        , ac_int<64, false>& cycles
    #endif
        )
{
    ac_int<32, false> next_pc;
    if(freeze_fetch)
    {
        next_pc = pc;               // read after load dependency, stall 1 cycle
    }
    else
    {
        next_pc = pc + 4;
    }
    ac_int<32, false> jump_pc;
    if(mem_lock > 1)
    {
        jump_pc = next_pc;          // this means that we already had a jump before, so prevent double jumping when 2 jumps or branch in a row
    }
    else
    {
        jump_pc = extoMem.memValue; // forward the value from ex stage for jump & branch
    }
    bool control = 0;
    switch(extoMem.opCode)
    {
    case RISCV_BR:
        control = extoMem.result > 0 ? 1 : 0;   // result is the result of the comparison so either 1 or 0, memvalue is the jump address
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
    if(!freeze_fetch)
    {
        if(insvalid && cachepc == pc)
        {
            ftoDC.instruction = instruction;
            ftoDC.pc = pc;
            if(control)
            {
                pc = jump_pc % N;
            }
            else
            {
                pc = next_pc % N;
            }
            debug("%06x\n", pc.to_int());
        }
        else
        {
            static bool init = false;
            ftoDC.instruction = 0x13;  // insert bubble (0x13 is NOP instruction and corresponds to ADDI r0, r0, 0)
            ftoDC.pc = 0;

            if(!init)
            {
                pc %= N;
                init = true;
            }
            else if(mem_lock > 1)   // there was a jump ealier, so we do nothing because right instruction still hasn't been fetched
            {
                debug("Had a jump, not moving & ");
            }
            // if there's a jump at the same time of a miss, update to jump_pc
            else if(control)
            {
                pc = jump_pc % N;
                debug("Jumping & ");
            }
            debug("Requesting @%06x\n", pc.to_int());
        }
        iaddress = pc;
    }
    else      // we cannot overwrite because it would not execute the frozen instruction
    {
        ftoDC.instruction = ftoDC.instruction;
        ftoDC.pc = ftoDC.pc;
        debug("Fetch frozen, what to do?");
    }

    simul(if(ftoDC.pc)
    {
        coredebug("Ft   @%06x   %08x\n", ftoDC.pc.to_int(), ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
#else
    if(!freeze_fetch)
    {
        ftoDC.instruction = ins_memory[pc/4];
        simul(cycles += MEMORY_READ_LATENCY);
        ftoDC.pc = pc;
        //debug("i @%06x (%06x)   %08x    S:3\n", pc.to_int(), pc.to_int()/4, ins_memory[pc/4]);
    }
    else      // we cannot overwrite because it would not execute the frozen instruction
    {
        ftoDC.instruction = ftoDC.instruction;
        ftoDC.pc = ftoDC.pc;
        debug("Fetch frozen, what to do?");
    }
    simul(if(ftoDC.pc)
    {
        coredebug("Ft   @%06x   %08x\n", ftoDC.pc.to_int(), ftoDC.instruction.to_int());
    }
    else
    {
        coredebug("Ft   \n");
    })
    pc = control ? jump_pc : next_pc;
#endif
}

void DC(ac_int<32, true> REG[32], FtoDC ftoDC, ExtoMem extoMem, MemtoWB memtoWB, DCtoEx& dctoEx, ac_int<7, false>& prev_opCode,
        ac_int<32, false>& prev_pc, ac_int<3, false> mem_lock, bool& freeze_fetch, bool& ex_bubble, CSR csrs)
{
    ac_int<5, false> rs1 = ftoDC.instruction.slc<5>(15);       // Decoding the instruction, in the DC stage
    ac_int<5, false> rs2 = ftoDC.instruction.slc<5>(20);
    ac_int<5, false> rd = ftoDC.instruction.slc<5>(7);
    ac_int<7, false> opcode = ftoDC.instruction.slc<7>(0);
    ac_int<7, false> funct7 = ftoDC.instruction.slc<7>(25);
    ac_int<7, false> funct3 = ftoDC.instruction.slc<3>(12);
    ac_int<6, false> shamt = ftoDC.instruction.slc<5>(20);
    ac_int<13, false> imm13 = 0;
    imm13[12] = ftoDC.instruction[31];
    imm13.set_slc(5, ftoDC.instruction.slc<6>(25));
    imm13.set_slc(1, ftoDC.instruction.slc<4>(8));
    imm13[11] = ftoDC.instruction[7];
    ac_int<13, true> imm13_signed = 0;
    imm13_signed.set_slc(0, imm13);
    ac_int<12, false> imm12_I = ftoDC.instruction.slc<12>(20);
    ac_int<12, true> imm12_I_signed = ftoDC.instruction.slc<12>(20);
    ac_int<21, false> imm21_1 = 0;
    imm21_1.set_slc(12, ftoDC.instruction.slc<8>(12));
    imm21_1[11] = ftoDC.instruction[20];
    imm21_1.set_slc(1, ftoDC.instruction.slc<10>(21));
    imm21_1[20] = ftoDC.instruction[31];
    ac_int<21, true> imm21_1_signed = 0;
    imm21_1_signed.set_slc(0, imm21_1);
    ac_int<32, true> imm31_12 = 0;
    imm31_12.set_slc(12, ftoDC.instruction.slc<20>(12));
    bool forward_rs1 = false;
    bool forward_rs2 = false;
    bool forward_ex_or_mem_rs1 = false;
    bool forward_ex_or_mem_rs2 = false;
    bool datab_fwd = 0;
    ac_int<12, true> store_imm = 0;
    store_imm.set_slc(0,ftoDC.instruction.slc<5>(7));
    store_imm.set_slc(5,ftoDC.instruction.slc<7>(25));

    dctoEx.opCode = opcode;
    dctoEx.funct3 = funct3;
    dctoEx.funct7 = funct7;
    dctoEx.shamt = shamt;
    dctoEx.rs1 = rs1;
    dctoEx.rs2 = 0;
    dctoEx.pc = ftoDC.pc;
    dctoEx.instruction = ftoDC.instruction;
    freeze_fetch = 0;
    switch (opcode)
    {
    case RISCV_LUI:
        dctoEx.dest = rd;
        dctoEx.datab = imm31_12;
        break;
    case RISCV_AUIPC:
        dctoEx.dest = rd;
        dctoEx.datab = imm31_12;
        break;
    case RISCV_JAL:
        dctoEx.dest = rd;
        dctoEx.datab = imm21_1_signed;
        break;
    case RISCV_JALR:
        dctoEx.dest = rd;
        dctoEx.datab = imm12_I_signed;
        break;
    case RISCV_BR:
        dctoEx.rs2 = rs2;
        datab_fwd = 1;
        dctoEx.datac = imm13_signed;
        dctoEx.dest = 0;
        break;
    case RISCV_LD:
        dctoEx.dest = rd;
        dctoEx.memValue = imm12_I_signed;
        break;
    case RISCV_ST:
        dctoEx.datad = rs2;
        dctoEx.memValue = store_imm;
        dctoEx.dest = 0;
        break;
    case RISCV_OPI:
        dctoEx.dest = rd;
        dctoEx.memValue = imm12_I_signed;
        dctoEx.datab = imm12_I;
        break;
    case RISCV_OP:
        dctoEx.rs2 = rs2;
        datab_fwd = 1;
        dctoEx.dest = rd;
        break;
    case RISCV_SYSTEM:
        if(dctoEx.funct3 == RISCV_SYSTEM_ENV)
        {
            datab_fwd = 1;
            dctoEx.dest = 10;
            rs1 = 17;
            rs2 = 10;
            dctoEx.datac = (extoMem.dest == 11 && mem_lock < 2) ? extoMem.result : ((memtoWB.dest == 11 && mem_lock == 0) ? memtoWB.result : REG[11]);
            dctoEx.datad = (extoMem.dest == 12 && mem_lock < 2) ? extoMem.result : ((memtoWB.dest == 12 && mem_lock == 0) ? memtoWB.result : REG[12]);
            dctoEx.datae = (extoMem.dest == 13 && mem_lock < 2) ? extoMem.result : ((memtoWB.dest == 13 && mem_lock == 0) ? memtoWB.result : REG[13]);
        }
        else simul(if(dctoEx.funct3 != 0x4))
        {

            simul(switch(imm12_I.slc<2>(8))
            {
            case 0:
                fprintf(stderr, "User level CSR not implemented\n");
                assert(false && "User level CSR not implemented\n");
                break;
            case 1:
                fprintf(stderr, "Supervisor level CSR not implemented\n");
                assert(false && "Supervisor level CSR not implemented\n");
                break;
            case 2:
                fprintf(stderr, "Reserved CSR not implemented\n");
                assert(false && "Reserved CSR not implemented\n");
                break;
            case 3:
                break;
            })

            // handle the case for rd = 0 for CSRRW
            // handle WIRI/WARL/etc.
            switch(imm12_I)
            {
            case RISCV_CSR_MSTATUS:
                dctoEx.datab = csrs.mstatus;
                break;
            case RISCV_CSR_MISA:
                dctoEx.datab = csrs.misa;
                break;
            case RISCV_CSR_MEDELEG:
                dctoEx.datab = csrs.medeleg;
                break;
            case RISCV_CSR_MIDELEG:
                dctoEx.datab = csrs.mideleg;
                break;
            case RISCV_CSR_MIE:
                dctoEx.datab = csrs.mie;
                break;
            case RISCV_CSR_MTVEC:
                dctoEx.datab = csrs.mtvec;
                break;
            case RISCV_CSR_MCOUNTEREN:
                dctoEx.datab = csrs.mcounteren;
                break;
            case RISCV_CSR_MSCRATCH:
                dctoEx.datab = csrs.mscratch;
                break;
            case RISCV_CSR_MEPC:
                dctoEx.datab = csrs.mepc;
                break;
            case RISCV_CSR_MCAUSE:
                dctoEx.datab = csrs.mcause;
                break;
            case RISCV_CSR_MTVAL:
                dctoEx.datab = csrs.mtval;
                break;
            case RISCV_CSR_MIP:
                dctoEx.datab = csrs.mip;
                break;
            case RISCV_CSR_MCYCLE:
                dctoEx.datab = csrs.mcycle.slc<32>(0);
                break;
            case RISCV_CSR_MINSTRET:
                dctoEx.datab = csrs.minstret.slc<32>(0);
                break;
            case RISCV_CSR_MCYCLEH:
                dctoEx.datab = csrs.mcycle.slc<32>(32);
                break;
            case RISCV_CSR_MINSTRETH:
                dctoEx.datab = csrs.minstret.slc<32>(32);
                break;
            case RISCV_CSR_MVENDORID:
                dctoEx.datab = csrs.mvendorid;
                break;
            case RISCV_CSR_MARCHID:
                dctoEx.datab = csrs.marchid;
                break;
            case RISCV_CSR_MIMPID:
                dctoEx.datab = csrs.mimpid;
                break;
            case RISCV_CSR_MHARTID:
                dctoEx.datab = csrs.mhartid;
                break;
            default:
                fprintf(stderr, "Unknown CSR id : @%03x     @%06x\n", imm12_I.to_int(), dctoEx.pc.to_int());
                assert(false && "Unknown CSR id\n");
                break;
            }
            fprintf(stderr, "Reading %08x in CSR @%03x    @%06x\n", dctoEx.datab.to_int(), imm12_I.to_int(), dctoEx.pc.to_int());
            dctoEx.memValue = imm12_I;
            dctoEx.dest = rd;
            //dataa will contain REG[rs1]
        }
        simul(else assert(false));
        break;
    }

    ac_int<32, true> reg_rs1 = REG[rs1.slc<5>(0)];
    ac_int<32, true> reg_rs2 = REG[rs2.slc<5>(0)];

    forward_rs1 = ((extoMem.dest == rs1 && mem_lock < 2) || (memtoWB.dest == rs1 && mem_lock == 0)) ? 1 : 0;
    forward_rs2 = ((extoMem.dest == rs2 && mem_lock < 2) || (memtoWB.dest == rs2 && mem_lock == 0)) ? 1 : 0;
    forward_ex_or_mem_rs1 = (extoMem.dest == rs1) ? 1 : 0;
    forward_ex_or_mem_rs2 = (extoMem.dest == rs2) ? 1 : 0;

    dctoEx.dataa = (forward_rs1 && rs1 != 0) ? (forward_ex_or_mem_rs1 ? extoMem.result : memtoWB.result) : reg_rs1;

    if(opcode == RISCV_ST)
    {
        dctoEx.datac = (forward_rs2 && rs2 != 0) ? (forward_ex_or_mem_rs2 ? extoMem.result : memtoWB.result) : reg_rs2;
    }
    if(datab_fwd)
    {
        dctoEx.datab = (forward_rs2 && rs2 != 0) ? (forward_ex_or_mem_rs2 ? extoMem.result : memtoWB.result) : reg_rs2;
    }
    // stall 1 cycle if load to one of the register we use, e.g lw a5, someaddress followed by any operation that use a5
    if(prev_opCode == RISCV_LD && (extoMem.dest == rs1 || (opcode != RISCV_LD && extoMem.dest == rs2)) && mem_lock < 2 && prev_pc != ftoDC.pc)
    {
        freeze_fetch = 1;
        ex_bubble = 1;
    }
    // add a stall for CSRs instruction because they should be atomic? Detect if 2 CSRs follow each other
    prev_opCode = opcode;
    prev_pc = ftoDC.pc;

    simul(if(dctoEx.pc)
    {
        coredebug("Dc   @%06x   %08x\n", dctoEx.pc.to_int(), dctoEx.instruction.to_int());
    }
    else
    {
        coredebug("Dc   \n");
    })
}

void Ex(DCtoEx dctoEx, ExtoMem& extoMem, bool& ex_bubble, bool& mem_bubble
    #ifndef __SYNTHESIS__
        , Simulator* sim
    #endif
        )
{
    ac_int<32, false> unsignedReg1 = 0;
    unsignedReg1.set_slc(0,(dctoEx.dataa).slc<32>(0));
    ac_int<32, false> unsignedReg2 = 0;
    unsignedReg2.set_slc(0,(dctoEx.datab).slc<32>(0));
    ac_int<33, true> mul_reg_a = 0;
    ac_int<33, true> mul_reg_b = 0;
    ac_int<66, true> longResult = 0;
    extoMem.pc = dctoEx.pc;
    extoMem.instruction = dctoEx.instruction;
    extoMem.opCode = dctoEx.opCode;
    extoMem.dest = dctoEx.dest;
    extoMem.datac = dctoEx.datac;
    extoMem.funct3 = dctoEx.funct3;
    extoMem.datad = dctoEx.datad;
    extoMem.sys_status = 0;
    if ((extoMem.opCode != RISCV_BR) && (extoMem.opCode != RISCV_ST) && (extoMem.opCode != RISCV_MISC_MEM))
    {
        extoMem.WBena = 1;
    }
    else
    {
        extoMem.WBena = 0;
    }

    switch (dctoEx.opCode)
    {
    case RISCV_LUI:
        extoMem.result = dctoEx.datab;
        break;
    case RISCV_AUIPC:
        extoMem.result = dctoEx.pc + dctoEx.datab;
        break;
    case RISCV_JAL:
        extoMem.result = dctoEx.pc + 4;
        extoMem.memValue = dctoEx.pc + dctoEx.datab;
        break;
    case RISCV_JALR:
        extoMem.result = dctoEx.pc + 4;
        extoMem.memValue = (dctoEx.dataa + dctoEx.datab) & 0xfffffffe;
        break;
    case RISCV_BR: // Switch case for branch instructions
        switch(dctoEx.funct3)
        {
        case RISCV_BR_BEQ:
            extoMem.result = (dctoEx.dataa == dctoEx.datab);
            break;
        case RISCV_BR_BNE:
            extoMem.result = (dctoEx.dataa != dctoEx.datab);
            break;
        case RISCV_BR_BLT:
            extoMem.result = (dctoEx.dataa < dctoEx.datab);
            break;
        case RISCV_BR_BGE:
            extoMem.result = (dctoEx.dataa >= dctoEx.datab);
            break;
        case RISCV_BR_BLTU:
            extoMem.result = (unsignedReg1 < unsignedReg2);
            break;
        case RISCV_BR_BGEU:
            extoMem.result = (unsignedReg1 >= unsignedReg2);
            break;
        EXDEFAULT();
        }
        extoMem.memValue = dctoEx.pc + dctoEx.datac;
        break;
    case RISCV_LD:
        extoMem.result = (dctoEx.dataa + dctoEx.memValue);
        break;
    case RISCV_ST:
        extoMem.result = (dctoEx.dataa + dctoEx.memValue);
        extoMem.datac = dctoEx.datac;
        break;
    case RISCV_OPI:
        switch(dctoEx.funct3)
        {
        case RISCV_OPI_ADDI:
            extoMem.result = dctoEx.dataa + dctoEx.memValue;
            break;
        case RISCV_OPI_SLTI:
            extoMem.result = (dctoEx.dataa < dctoEx.memValue) ? 1 : 0;
            break;
        case RISCV_OPI_SLTIU:
            extoMem.result = (unsignedReg1 < dctoEx.datab) ? 1 : 0;
            break;
        case RISCV_OPI_XORI:
            extoMem.result = dctoEx.dataa ^ dctoEx.memValue;
            break;
        case RISCV_OPI_ORI:
            extoMem.result =  dctoEx.dataa | dctoEx.memValue;
            break;
        case RISCV_OPI_ANDI:
            extoMem.result = dctoEx.dataa & dctoEx.memValue;
            break;
        case RISCV_OPI_SLLI:
            extoMem.result = dctoEx.dataa << dctoEx.shamt;
            break;
        case RISCV_OPI_SRI:
            if (dctoEx.funct7.slc<1>(5))    //SRAI
                extoMem.result = dctoEx.dataa >> dctoEx.shamt;
            else                            //SRLI
                extoMem.result = ((ac_int<32, false>)dctoEx.dataa) >> dctoEx.shamt;
            //fprintf(stderr, "@%06x      %08x >> %02x = %08x\n", dctoEx.pc.to_int(), dctoEx.dataa.to_int(), dctoEx.shamt.to_int(), extoMem.result.to_int());
            break;
        EXDEFAULT();
        }
        break;
    case RISCV_OP:
        if(dctoEx.funct7.slc<1>(0))     // M Extension
        {
            mul_reg_a = dctoEx.dataa;
            mul_reg_b = dctoEx.datab;
            mul_reg_a[32] = dctoEx.dataa[31];
            mul_reg_b[32] = dctoEx.datab[31];
            switch (dctoEx.funct3)  //Switch case for multiplication operations (RV32M)
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
            switch(dctoEx.funct3)
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
                //fprintf(stderr, "DIV @%06x : %08x / %08x = %08x\n", dctoEx.pc.to_int(), mul_reg_a.to_int(), mul_reg_b.to_int(), longResult.to_int());
                break;
            case RISCV_OP_M_REM:
            case RISCV_OP_M_REMU:
                if(mul_reg_b)
                    longResult = mul_reg_a % mul_reg_b;
                else    // modulo 0 => result is first operand
                    longResult = mul_reg_a;
                //fprintf(stderr, "REM @%06x : %08x %% %08x = %08x\n", dctoEx.pc.to_int(), mul_reg_a.to_int(), mul_reg_b.to_int(), longResult.to_int());
                break;
            EXDEFAULT();
            }
        #else
            longResult = mul_reg_a * mul_reg_b;
        #endif
            if(dctoEx.funct3 == RISCV_OP_M_MULH || dctoEx.funct3 == RISCV_OP_M_MULHSU || dctoEx.funct3 == RISCV_OP_M_MULHU)
            {
                extoMem.result = longResult.slc<32>(32);
            }
            else
            {
                extoMem.result = longResult.slc<32>(0);
            }
        }
        else
        {
            switch(dctoEx.funct3)
            {
            case RISCV_OP_ADD:
                if (dctoEx.funct7.slc<1>(5))    // SUB
                    extoMem.result = dctoEx.dataa - dctoEx.datab;
                else                            // ADD
                    extoMem.result = dctoEx.dataa + dctoEx.datab;
                break;
            case RISCV_OP_SLL:
                extoMem.result = dctoEx.dataa << ((ac_int<5, false>)dctoEx.datab);
                break;
            case RISCV_OP_SLT:
                extoMem.result = (dctoEx.dataa < dctoEx.datab) ? 1 : 0;
                break;
            case RISCV_OP_SLTU:
                extoMem.result = (unsignedReg1 < unsignedReg2) ? 1 : 0;
                break;
            case RISCV_OP_XOR:
                extoMem.result = dctoEx.dataa ^ dctoEx.datab;
                break;
            case RISCV_OP_SR:
                if(dctoEx.funct7.slc<1>(5))     // SRA
                    extoMem.result = dctoEx.dataa >> ((ac_int<5, false>)dctoEx.datab);
                else                            // SRL
                    extoMem.result = ((ac_int<32, false>)dctoEx.dataa) >> ((ac_int<5, false>)dctoEx.datab);
                break;
            case RISCV_OP_OR:
                extoMem.result = dctoEx.dataa | dctoEx.datab;
                break;
            case RISCV_OP_AND:
                extoMem.result =  dctoEx.dataa & dctoEx.datab;
                break;
            EXDEFAULT();
            }
        }
        break;
    case RISCV_MISC_MEM:    // this does nothing because all memory accesses are ordered and we have only one core
        break;
    case RISCV_SYSTEM:
        if(dctoEx.funct3 == 0)
        {
        #ifndef __SYNTHESIS__
            extoMem.result = sim->solveSyscall(dctoEx.dataa, dctoEx.datab, dctoEx.datac, dctoEx.datad, dctoEx.datae, extoMem.sys_status);
            fprintf(stderr, "Syscall @%06x\n", dctoEx.pc.to_int());
        #endif
        }
        else
        {
            switch(dctoEx.funct3)   // dataa is from rs1, datab is from csr
            {   // case 0: mret instruction, dctoEx.memValue should be 0x302
            case RISCV_SYSTEM_CSRRW:
                extoMem.datac = dctoEx.dataa;       // written back to csr
                fprintf(stderr, "CSRRW @%03x    @%06x\n", dctoEx.memValue.to_int(), dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRS:
                extoMem.datac = dctoEx.dataa | dctoEx.datab;
                fprintf(stderr, "CSRRS @%03x    @%06x\n", dctoEx.memValue.to_int(), dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRC:
                extoMem.datac = ((ac_int<32, false>)~dctoEx.dataa) & dctoEx.datab;
                fprintf(stderr, "CSRRC @%03x    @%06x\n", dctoEx.memValue.to_int(), dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRWI:
                extoMem.datac = dctoEx.rs1;
                fprintf(stderr, "CSRRWI @%03x    @%06x\n", dctoEx.memValue.to_int(), dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRSI:
                extoMem.datac = dctoEx.rs1 | dctoEx.datab;
                fprintf(stderr, "CSRRSI @%03x    @%06x\n", dctoEx.memValue.to_int(), dctoEx.pc.to_int());
                break;
            case RISCV_SYSTEM_CSRRCI:
                extoMem.datac = ((ac_int<32, false>)~dctoEx.rs1) & dctoEx.datab;
                fprintf(stderr, "CSRRCI @%03x    @%06x\n", dctoEx.memValue.to_int(), dctoEx.pc.to_int());
                break;
            EXDEFAULT();
            }
            extoMem.result = dctoEx.datab;      // written back to rd
            extoMem.memValue = dctoEx.memValue;
            extoMem.rs1 = dctoEx.rs1;
        }
        break;
    EXDEFAULT();
    }

    if(ex_bubble)
    {
        mem_bubble = 1;
        extoMem.pc = 0;
        extoMem.result = 0; //Result of the EX stage
        extoMem.datad = 0;
        extoMem.datac = 0;
        extoMem.dest = 0;
        extoMem.WBena = 0;
        extoMem.opCode = 0;
        extoMem.memValue = 0;
        extoMem.rs1 = 0;
        extoMem.funct3 = 0;
    }
    ex_bubble = 0;

    simul(if(extoMem.pc)
    {
        coredebug("Ex   @%06x   %08x\n", extoMem.pc.to_int(), extoMem.instruction.to_int());
    }
    else
    {
        coredebug("Ex   \n");
    })

}

void do_Mem(ExtoMem extoMem, MemtoWB& memtoWB, ac_int<3, false>& mem_lock, bool& mem_bubble, bool& cachelock,  // internal core control
            ac_int<32, false>& address, ac_int<2, false>& datasize, bool& signenable, bool& cacheenable, bool& writeenable, int& writevalue,      // control & data to cache
            int readvalue, bool datavalid, unsigned int data_memory[N]
            #ifndef __SYNTHESIS__
                , ac_int<64, false>& cycles
            #endif
            )          // data & acknowledgment from cache
{
    if(cachelock)
    {
        if(datavalid)
        {
            memtoWB.WBena = extoMem.WBena;
            memtoWB.pc = extoMem.pc;
            if(!writeenable)
                memtoWB.result = readvalue;
            cachelock = false;
            cacheenable = false;
            writeenable = false;
        }
        else
        {
            memtoWB.pc = 0;
        }
    }
    else if(mem_bubble)
    {
        mem_bubble = 0;
        //wb_bubble = 1;
        memtoWB.pc = 0;
        memtoWB.instruction = 0;
        memtoWB.result = 0; //Result to be written back
        memtoWB.dest = 0; //Register to be written at WB stage
        memtoWB.WBena = 0; //Is a WB is needed ?
        memtoWB.opCode = 0;
        memtoWB.sys_status = 0;
    }
    else
    {
        if(mem_lock > 0)
        {
            mem_lock = mem_lock - 1;
            memtoWB.WBena = 0;
            simul(if(mem_lock)
                debug("I    @%06x\n", extoMem.pc.to_int());
            )
        }

        memtoWB.sys_status = extoMem.sys_status;
        memtoWB.opCode = extoMem.opCode;
        memtoWB.result = extoMem.result;
        memtoWB.CSRid = extoMem.memValue;
        memtoWB.rescsr = extoMem.datac;
        memtoWB.rs1 = extoMem.rs1;
        memtoWB.csrwb = false;

        bool sign = 0;

        if(mem_lock == 0)
        {
            memtoWB.pc = extoMem.pc;
            memtoWB.instruction = extoMem.instruction;
            memtoWB.WBena = extoMem.WBena;
            memtoWB.dest = extoMem.dest;
            switch(extoMem.opCode)
            {
            case RISCV_BR:
                if (extoMem.result)
                {
                    mem_lock = 3;
                }
                memtoWB.WBena = 0;
                memtoWB.dest = 0;
                break;
            case RISCV_JAL:
                mem_lock = 3;
                break;
            case RISCV_JALR:
                mem_lock = 3;
                break;
            case RISCV_LD:
                switch(extoMem.funct3)
                {
                case RISCV_LD_LW:
                    datasize = 3;
                    sign = 1;
                    break;
                case RISCV_LD_LH:
                    datasize = 1;
                    sign = 1;
                    break;
                case RISCV_LD_LHU:
                    datasize = 1;
                    sign = 0;
                    break;
                case RISCV_LD_LB:
                    datasize = 0;
                    sign = 1;
                    break;
                case RISCV_LD_LBU:
                    datasize = 0;
                    sign = 0;
                    break;
                }
#ifndef nocache
                address = memtoWB.result % N;
                signenable = sign;
                cacheenable = true;
                writeenable = false;
                cachelock = true;
                memtoWB.WBena = false;
                memtoWB.pc = 0;
#else
                //debug("%5d  ", cycles);
                memtoWB.result = memoryGet(data_memory, memtoWB.result, datasize, sign
                                       #ifndef __SYNTHESIS__
                                           , cycles
                                       #endif
                                           );
#endif
                break;
            case RISCV_ST:
                switch(extoMem.funct3)
                {
                case RISCV_ST_STW:
                    datasize = 3;
                    break;
                case RISCV_ST_STH:
                    datasize = 1;
                    break;
                case RISCV_ST_STB:
                    datasize = 0;
                    break;
                }
#ifndef nocache
                address = memtoWB.result % N;
                signenable = false;
                cacheenable = true;
                writeenable = true;
                writevalue = extoMem.datac;
                cachelock = true;
                memtoWB.WBena = false;
                memtoWB.pc = 0;
#else
                //debug("%5d  ", cycles);
                memorySet(data_memory, memtoWB.result, extoMem.datac, datasize
                      #ifndef __SYNTHESIS__
                          , cycles
                      #endif
                          );
#endif
                //data_memory[(memtoWB.result/4)%N] = extoMem.datac;
                break;
            case RISCV_SYSTEM:
                memtoWB.csrwb = true;
                break;
            }
        }
    }
    simul(if(memtoWB.pc)
    {
        coredebug("Mem  @%06x   %08x\n", memtoWB.pc.to_int(), memtoWB.instruction.to_int());
    }
    else
    {
        coredebug("Mem  \n");
    })
}

void doWB(ac_int<32, true> REG[32], MemtoWB& memtoWB, bool& early_exit, CSR& csrs)
{
    if (memtoWB.WBena == 1 && memtoWB.dest != 0)
    {
        REG[memtoWB.dest.slc<5>(0)] = memtoWB.result;
    }

    if(memtoWB.rs1 && memtoWB.csrwb)     // condition should be more precise
    {
        fprintf(stderr, "Writing %08x in CSR @%03x   @%06x\n", memtoWB.rescsr.to_int(), memtoWB.CSRid.to_int(), memtoWB.pc.to_int());
        switch(memtoWB.CSRid)
        {
        case RISCV_CSR_MSTATUS:
            csrs.mstatus = memtoWB.rescsr;
            break;
        case RISCV_CSR_MISA:
            csrs.misa = memtoWB.rescsr;
            break;
        case RISCV_CSR_MEDELEG:
            csrs.medeleg = memtoWB.rescsr;
            break;
        case RISCV_CSR_MIDELEG:
            csrs.mideleg = memtoWB.rescsr;
            break;
        case RISCV_CSR_MIE:
            csrs.mie = memtoWB.rescsr;
            break;
        case RISCV_CSR_MTVEC:
            csrs.mtvec = memtoWB.rescsr;
            break;
        case RISCV_CSR_MCOUNTEREN:
            csrs.mcounteren = memtoWB.rescsr;
            break;
        case RISCV_CSR_MSCRATCH:
            csrs.mscratch = memtoWB.rescsr;
            break;
        case RISCV_CSR_MEPC:
            csrs.mepc = memtoWB.rescsr;
            break;
        case RISCV_CSR_MCAUSE:
            csrs.mcause = memtoWB.rescsr;
            break;
        case RISCV_CSR_MTVAL:
            csrs.mtval = memtoWB.rescsr;
            break;
        case RISCV_CSR_MIP:
            csrs.mip = memtoWB.rescsr;
            break;
        case RISCV_CSR_MCYCLE:
            csrs.mcycle.set_slc(0, memtoWB.rescsr);
            break;
        case RISCV_CSR_MINSTRET:
            csrs.minstret.set_slc(0, memtoWB.rescsr);
            break;
        case RISCV_CSR_MCYCLEH:
            csrs.mcycle.set_slc(32, memtoWB.rescsr);
            break;
        case RISCV_CSR_MINSTRETH:
            csrs.minstret.set_slc(32, memtoWB.rescsr);
            break;
        case RISCV_CSR_MVENDORID:
            fprintf(stderr, "Read only CSR %03x", memtoWB.CSRid);
            break;
        case RISCV_CSR_MARCHID:
            fprintf(stderr, "Read only CSR %03x", memtoWB.CSRid);
            break;
        case RISCV_CSR_MIMPID:
            fprintf(stderr, "Read only CSR %03x", memtoWB.CSRid);
            break;
        case RISCV_CSR_MHARTID:
            fprintf(stderr, "Read only CSR %03x", memtoWB.CSRid);
            break;
        default:
            fprintf(stderr, "Unknown CSR id : %03x\n", memtoWB.CSRid);
            assert(false && "Unknown CSR id\n");
        }
    }

#ifndef __SYNTHESIS__
    if(memtoWB.sys_status == 1)
    {
        debug("\nExit system call received, Exiting...");
        early_exit = 1;
    }
    else if(memtoWB.sys_status == 2)
    {
        debug("\nUnknown system call received, Exiting...");
        early_exit = 1;
    }
#endif

    if(memtoWB.pc)
    {
        if(memtoWB.pc != memtoWB.lastpc)
        {
            csrs.minstret += 1;
            memtoWB.lastpc = memtoWB.pc;
        }
        coredebug("\nWB   @%06x   %08x   (%d)\n", memtoWB.pc.to_int(), memtoWB.instruction.to_int(), csrs.minstret.to_int64());
    }
    else
    {
        coredebug("\nWB   \n");
    }

}

void doStep(ac_int<32, false> startpc, unsigned int ins_memory[N], unsigned int dm[N], bool& exit
        #ifndef __SYNTHESIS__
            , ac_int<64, false>& c, ac_int<64, false>& numins, Simulator* sim
        #endif
            )
{
    static ICacheControl ictrl = {0};
    static unsigned int idata[Sets][Blocksize][Associativity] = {0};
#ifdef __SYNTHESIS__
    static bool idummy = ac::init_array<AC_VAL_DC>((unsigned int*)idata, Sets*Associativity*Blocksize);
    (void)idummy;
    static bool itaginit = ac::init_array<AC_VAL_DC>((ac_int<32-tagshift, false>*)ictrl.tag, Sets*Associativity);
    (void)itaginit;
    static bool ivalinit = ac::init_array<AC_VAL_0>((bool*)ictrl.valid, Sets*Associativity);
    (void)ivalinit;
#endif
    static DCacheControl dctrl = {0};
    static unsigned int ddata[Sets][Blocksize][Associativity] = {0};
#ifdef __SYNTHESIS__
    static bool dummy = ac::init_array<AC_VAL_DC>((unsigned int*)ddata, Sets*Associativity*Blocksize);
    (void)dummy;
    static bool taginit = ac::init_array<AC_VAL_DC>((ac_int<32-tagshift, false>*)dctrl.tag, Sets*Associativity);
    (void)taginit;
    static bool dirinit = ac::init_array<AC_VAL_DC>((bool*)dctrl.dirty, Sets*Associativity);
    (void)dirinit;
    static bool valinit = ac::init_array<AC_VAL_0>((bool*)dctrl.valid, Sets*Associativity);
    (void)valinit;
#if Policy == FIFO
    static bool dpolinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)dctrl.policy, Sets);
    (void)dpolinit;
    static bool ipolinit = ac::init_array<AC_VAL_DC>((ac_int<ac::log2_ceil<Associativity>::val, false>*)ictrl.policy, Sets);
    (void)ipolinit;
#elif Policy == LRU
    static bool dpolinit = ac::init_array<AC_VAL_DC>((ac_int<Associativity * (Associativity-1) / 2, false>*)dctrl.policy, Sets);
    (void)dpolinit;
    static bool ipolinit = ac::init_array<AC_VAL_DC>((ac_int<Associativity * (Associativity-1) / 2, false>*)ictrl.policy, Sets);
    (void)ipolinit;
#elif Policy == RANDOM
    static bool rndinit = false;
    if(!rndinit)
    {
        rndinit = true;
        dctrl.policy = 0xF2D4B698;
        ictrl.policy = 0xF2D4B698;
    }
#endif
#endif  // __SYNTHESIS__
    static MemtoWB memtoWB = {0};
    static ExtoMem extoMem = {0};
    static DCtoEx dctoEx = {0}; // opCode = 0x13
    static FtoDC ftoDC = {0};

    static CSR csrs = {0};

    static ac_int<32, true> REG[32] = {0};/*,0,0xf0000,0,0,0,0,0,   // 0xf0000 is stack address and so is the highest reachable address
                                       0,0,0,0,0,0,0,0,         // we can put whatever needed value for the stack
                                       0,0,0,0,0,0,0,0,
                                       0,0,0,0,0,0,0,0};*/

    static ac_int<7, false> prev_opCode = 0;
    static ac_int<32, false> prev_pc = 0;

    static ac_int<3, false> mem_lock = 0;
    static bool early_exit = false;

    static bool freeze_fetch = false;
    static bool ex_bubble = false;
    static bool mem_bubble = false;
    static bool cachelock = false;
    static ac_int<32, false> pc = 0;
    static bool init = false;
    if(!init)
    {
        init = true;
        pc = startpc;   // startpc - 4 ?

        memtoWB.instruction = memtoWB.opCode = 0x13;
        extoMem.instruction = extoMem.opCode = 0x13;
        dctoEx.instruction = dctoEx.opCode = 0x13;
        ftoDC.instruction = 0x13;
        REG[2] = STACK_INIT;

        simul(sim->setCore(REG, &dctrl, ddata));
    }

    /// Instruction cache
    static ac_int<32, false> iaddress = 0;
    static ac_int<32, false> cachepc = 0;
    static int instruction = 0;
    static bool insvalid = false;

    /// Data cache
    static ac_int<32, false> daddress = 0;
    static ac_int<2, false> datasize = 0;
    static bool signenable = false;
    static bool dcacheenable = false;
    static bool writeenable = false;
    static int writevalue = 0;
    static int readvalue = 0;
    static bool datavalid = false;

    simul(uint64_t oldcycles = csrs.mcycle);

    doWB(REG, memtoWB, early_exit, csrs);
    simul(coredebug("%d ", csrs.mcycle.to_int64());
    for(int i=0; i<32; i++)
    {
        if(REG[i])
            coredebug("%d:%08x ", i, (int)REG[i]);
    }
    )
    coredebug("\n");

    do_Mem(extoMem, memtoWB, mem_lock, mem_bubble, cachelock,                // internal core control
           daddress, datasize, signenable, dcacheenable, writeenable, writevalue,       // control & data to cache
           readvalue, datavalid, dm                                                     // data & acknowledgment from cache
       #ifndef __SYNTHESIS__
           , csrs.mcycle
       #endif
           );

#ifndef nocache
    dcache(dctrl, dm, ddata, daddress, datasize, signenable, dcacheenable, writeenable, writevalue, readvalue, datavalid
       #ifndef __SYNTHESIS__
           , csrs.mcycle
       #endif
           );
#endif

    if(!cachelock)
    {
        Ex(dctoEx, extoMem, ex_bubble, mem_bubble
   #ifndef __SYNTHESIS__
           , sim
   #endif
           );
        DC(REG, ftoDC, extoMem, memtoWB, dctoEx, prev_opCode, prev_pc, mem_lock, freeze_fetch, ex_bubble, csrs);
        Ft(pc, freeze_fetch, extoMem, ftoDC, mem_lock, iaddress, cachepc, instruction, insvalid, ins_memory
   #ifndef __SYNTHESIS__
          , csrs.mcycle
   #endif
          );
    }

    // icache is outside because it can still continues fetching
#ifndef nocache
    icache(ictrl, ins_memory, idata, iaddress, cachepc, instruction, insvalid
       #ifndef __SYNTHESIS__
           , csrs.mcycle
       #endif
           );
#endif

    csrs.mcycle++;

    simul(
    int M = MEMORY_READ_LATENCY>MEMORY_WRITE_LATENCY?MEMORY_READ_LATENCY:MEMORY_WRITE_LATENCY;
    if(oldcycles + M < csrs.mcycle)
    {
        csrs.mcycle = oldcycles + M; // we cannot step slower than the worst latency
    }
    c = csrs.mcycle;
    numins = csrs.minstret;
    )


    // riscv specification v2.2 p11
    assert((pc.to_int() & 3) == 0 && "An instruction address misaligned exception is generated on a taken branch or unconditional jump if the target address is not four-byte aligned.");

    simul(
    if(early_exit)
    {
        exit = true;
    }
    #ifndef nocache
        //cache write back for simulation
        for(unsigned int i  = 0; i < Sets; ++i)
            for(unsigned int j = 0; j < Associativity; ++j)
                if(dctrl.dirty[i][j] && dctrl.valid[i][j])
                    for(unsigned int k = 0; k < Blocksize; ++k)
                        dm[(dctrl.tag[i][j].to_int() << (tagshift-2)) | (i << (setshift-2)) | k] = ddata[i][k][j];
    #endif
    )
}
