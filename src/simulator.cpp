#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "simulator.h"

#ifndef nodbgsys
#define dbgsys(format, ...)     fprintf(stderr, "Syscall (%lld) : " format " from core %d\n", core[currentcore]->csrs.minstret.to_int64(), ## __VA_ARGS__, currentcore)
#else
#define dbgsys(...)
#endif

Simulator::Simulator(const char* binaryFile, const char* inputFile, const char* outputFile, int benchargc, char **benchargv)
    : core(0), dctrl(0), ddata(0), currentcore(-1)
{
    ins_memory = new ac_int<32, true>[DRAM_SIZE];
    data_memory = new ac_int<32, true>[DRAM_SIZE];
    for(int i(0); i < DRAM_SIZE; i++)
    {
        ins_memory[i] = 0;
        data_memory[i] = 0;
    }
    heapAddress = 0;
    input = output = 0;

    if(inputFile)
        input = fopen(inputFile, "rb");
    if(outputFile)
        output = fopen(outputFile, "wb");

    ElfFile elfFile(binaryFile);
    int counter = 0;
    for(unsigned int sectionCounter = 0; sectionCounter < elfFile.sectionTable->size(); sectionCounter++)
    {
        ElfSection *oneSection = elfFile.sectionTable->at(sectionCounter);
        if(oneSection->address != 0 && strncmp(oneSection->getName().c_str(), ".text", 5))
        {
            //If the address is not null we place its content into memory
            unsigned char* sectionContent = oneSection->getSectionCode();
            for(unsigned int byteNumber = 0; byteNumber<oneSection->size; byteNumber++)
            {
                counter++;
                setDataMemory(oneSection->address + byteNumber, sectionContent[byteNumber]);
            }
            free(sectionContent);
            coredebug("filling data from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }

        if(!strncmp(oneSection->getName().c_str(), ".text", 5))
        {
            unsigned char* sectionContent = oneSection->getSectionCode();
            for(unsigned int byteNumber = 0; byteNumber < oneSection->size; byteNumber++)
            {
                setInstructionMemory((oneSection->address + byteNumber), sectionContent[byteNumber]);
            }
            free(sectionContent);
            coredebug("filling instruction from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }
    }

    for(int oneSymbol = 0; oneSymbol < elfFile.symbols->size(); oneSymbol++)
    {
        ElfSymbol *symbol = elfFile.symbols->at(oneSymbol);
        unsigned char* sectionContent = elfFile.sectionTable->at(elfFile.indexOfSymbolNameSection)->getSectionCode();
        const char* name = (const char*) &(sectionContent[symbol->name]);
        if(strcmp(name, "_start") == 0)
        {
            fprintf(stderr, "%s     @%06x\n", name, symbol->offset);
            setPC(symbol->offset);
        }

        if(strcmp(name, "_exit") == 0)
        {
            fprintf(stderr, "%s     @%06x\n", name, symbol->offset);
            exit = symbol->offset;
        }

        if(strcmp(name, "tohost") == 0)
        {
            dbglog("tohost @%06x\n", symbol->offset);
        }

        if(strcmp(name, "fromhost") == 0)
        {
            dbglog("fromhost @%06x\n", symbol->offset);
        }
        free(sectionContent);
    }

    unsigned int heap = heapAddress;    // keep heap where it is because it will be set over stackpointer

    setDataMemory(STACK_INIT, benchargc & 0xFF);
    setDataMemory(STACK_INIT + 1, (benchargc >> 8) & 0xFF);
    setDataMemory(STACK_INIT + 2, (benchargc >> 16) & 0xFF);
    setDataMemory(STACK_INIT + 3, (benchargc >> 24) & 0xFF);
    //fprintf(stderr, "Writing %08x @%06x\n", benchargc, STACK_INIT);

    ac_int<32, true> currentPlaceStrings = STACK_INIT + 4 + 4*benchargc;
    for (int oneArg = 0; oneArg < benchargc; oneArg++)
    {
        setDataMemory(STACK_INIT+ 4*oneArg + 4, currentPlaceStrings.slc<8>(0));
        setDataMemory(STACK_INIT+ 4*oneArg + 5, currentPlaceStrings.slc<8>(8));
        setDataMemory(STACK_INIT+ 4*oneArg + 6, currentPlaceStrings.slc<8>(16));
        setDataMemory(STACK_INIT+ 4*oneArg + 7, currentPlaceStrings.slc<8>(24));
        //fprintf(stderr, "Writing %08x @%06x\n", (int)currentPlaceStrings, STACK_INIT+ 4*oneArg + 4);

        int oneCharIndex = 0;
        char oneChar = benchargv[oneArg][oneCharIndex];
        while (oneChar != 0)
        {
            setDataMemory(currentPlaceStrings + oneCharIndex, oneChar);

            //fprintf(stderr, "Writing %c (%d) @%06x\n", oneChar, (int)oneChar, currentPlaceStrings.to_int() + oneCharIndex);

            oneCharIndex++;
            oneChar = benchargv[oneArg][oneCharIndex];
        }
        setDataMemory(currentPlaceStrings + oneCharIndex, oneChar);
        //fprintf(stderr, "Writing %c (%d) @%06x\n", oneChar, (int)oneChar, currentPlaceStrings.to_int() + oneCharIndex);
        oneCharIndex++;
        currentPlaceStrings += oneCharIndex;
    }

    fillMemory();
    heapAddress = heap;
}

Simulator::~Simulator()
{
    delete[] ins_memory;
    delete[] data_memory;

    if(input)
        fclose(input);
    if(output)
        fclose(output);
}

void Simulator::fillMemory()
{
    //Check whether data memory and instruction memory from program will actually fit in processor.
    //cout << ins_memorymap.size()<<endl;

    if(ins_memorymap.size() / 4 > DRAM_SIZE)
    {
        printf("Error! Instruction memory size exceeded");
        std::exit(-1);
    }
    if(data_memorymap.size() / 4 > DRAM_SIZE)
    {
        printf("Error! Data memory size exceeded");
        std::exit(-1);
    }

    //fill instruction memory
    for(std::map<ac_int<32, false>, ac_int<8, false> >::iterator it = ins_memorymap.begin(); it!=ins_memorymap.end(); ++it)
    {
        ins_memory[(it->first.to_uint()/4)].set_slc(((it->first.to_uint())%4)*8,it->second);
        //gdebug("@%06x    @%06x    %d    %02x\n", it->first, (it->first/4) % DRAM_SIZE, ((it->first)%4)*8, it->second);
    }

    //fill data memory
    for(std::map<ac_int<32, false>, ac_int<8, false> >::iterator it = data_memorymap.begin(); it!=data_memorymap.end(); ++it)
    {
        //data_memory.set_byte((it->first/4)%DRAM_SIZE,it->second,it->first%4);
        data_memory[(it->first.to_uint()/4)].set_slc(((it->first.to_uint())%4)*8,it->second);
    }
}

void Simulator::setInstructionMemory(ac_int<32, false> addr, ac_int<8, true> value)
{
    ins_memorymap[addr] = value;
    if(addr > heapAddress)
        heapAddress = addr;
}

void Simulator::setDataMemory(ac_int<32, false> addr, ac_int<8, true> value)
{
    data_memorymap[addr] = value;
    if(addr > heapAddress)
        heapAddress = addr;
}

void Simulator::setDM(unsigned int *d)
{
    dm = d;
}

void Simulator::setIM(unsigned int *i)
{
    im = i;
}

void Simulator::writeBack()
{
#ifndef nocache
    for(currentcore = 0; currentcore < doNbcore(); ++currentcore)
    {
        unsigned int (&cdm)[Sets][Blocksize][Associativity] = (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(ddata[currentcore]));

        //cache write back for simulation
        for(unsigned int i  = 0; i < Sets; ++i)
            for(unsigned int j = 0; j < Associativity; ++j)
                                // dirty bit                                    // valid bit
                if(dctrl[currentcore][i].slc<1>(Associativity*(32-tagshift+1) + j) && dctrl[currentcore][i].slc<1>(Associativity*(32-tagshift) + j))
                    for(unsigned int k = 0; k < Blocksize; ++k)
                        dm[(dctrl[currentcore][i].slc<32-tagshift>(j*(32-tagshift)).to_int() << (tagshift-2)) | (i << (setshift-2)) | k] = cdm[i][k][j];
    }
#endif
}

void Simulator::setCore(Core *c, ac_int<DWidth, false>* ctrl, unsigned int cachedata[Sets][Blocksize][Associativity])
{
    if(c)
        core.push_back(c);
    dctrl.push_back(ctrl);
    ddata.push_back((unsigned int*)cachedata);
}

void Simulator::setCore(Core* c)
{
    core.push_back(c);
}

ac_int<32, true>* Simulator::getInstructionMemory() const
{
    return ins_memory;
}

ac_int<32, true>* Simulator::getDataMemory() const
{
    return data_memory;
}

Core* Simulator::getCore(int id) const
{
    return core[id];
}

void Simulator::setPC(ac_int<32, false> pc)
{
    this->pc = pc;
}

ac_int<32, false> Simulator::getPC() const
{
    return pc;
}

void Simulator::stb(ac_int<32, false> addr, ac_int<8, true> value)
{
#ifndef nocache     // if data is in the cache, we must write in the cache directly
    int i = getSet(addr);
    for(int j(0); j < Associativity; ++j)
    {
        if(dctrl[currentcore][i].slc<32-tagshift>(j*(32-tagshift)) == getTag(addr))
        {
            ac_int<32, false> mem = ddata[currentcore][i*Blocksize*Associativity + (int)getOffset(addr)*Associativity + j];
            formatwrite(addr, 0, mem, value);
            ddata[currentcore][i*Blocksize*Associativity + (int)getOffset(addr)*Associativity + j] = mem;
            dctrl[currentcore][i].set_slc(Associativity*(32-tagshift+1) + j, (ac_int<1, false>)true);      // mark as dirty because we wrote it
            //fprintf(stderr, "data @%06x (%06x) is in cache\n", addr.to_int(), dctrl[currentcore]->tag[i][j].to_int());
        }
    }
#endif
    // write in main memory as well
    ac_int<32, false> mem = dm[addr >> 2];
    formatwrite(addr, 0, mem, value);
    dm[addr >> 2] = mem;
    //fprintf(stderr,"Write @%06x   %02x\n", addr.to_int(), value.to_int()&0xFF);

}

void Simulator::sth(ac_int<32, false> addr, ac_int<16, true> value)
{
    this->stb(addr+1, value.slc<8>(8));
    this->stb(addr+0, value.slc<8>(0));
}

void Simulator::stw(ac_int<32, false> addr, ac_int<32, true> value)
{
    this->stb(addr+3, value.slc<8>(24));
    this->stb(addr+2, value.slc<8>(16));
    this->stb(addr+1, value.slc<8>(8));
    this->stb(addr+0, value.slc<8>(0));
}

void Simulator::std(ac_int<32, false> addr, ac_int<64, true> value)
{
    this->stb(addr+7, value.slc<8>(56));
    this->stb(addr+6, value.slc<8>(48));
    this->stb(addr+5, value.slc<8>(40));
    this->stb(addr+4, value.slc<8>(32));
    this->stb(addr+3, value.slc<8>(24));
    this->stb(addr+2, value.slc<8>(16));
    this->stb(addr+1, value.slc<8>(8));
    this->stb(addr+0, value.slc<8>(0));
}


ac_int<8, true> Simulator::ldb(ac_int<32, false> addr)
{
#ifndef nocache     // if data is in the cache, we must read in the cache directly
    int i = getSet(addr);
    for(int j(0); j < Associativity; ++j)
    {
        if(dctrl[currentcore][i].slc<32-tagshift>(j*(32-tagshift)) == getTag(addr))
        {
            ac_int<32, false> mem = ddata[currentcore][i*Blocksize*Associativity + (int)getOffset(addr)*Associativity + j];
            formatread(addr, 0, 0, mem);
            //fprintf(stderr, "data @%06x (%06x) is in cache\n", addr.to_int(), dctrl[currentcore]->tag[i][j].to_int());
            return mem;
        }
    }
#endif
    // read main memory if it wasn't in cache
    ac_int<8, true> result;
    ac_int<32, false> read = dm[addr >> 2];
    formatread(addr, 0, 0, read);
    result = read;
    //fprintf(stderr, "Read @%06x    %02x   %08x\n", addr.to_int(), result.to_int(), dm[addr >> 2]);
    return result;

}


//Little endian version
ac_int<16, true> Simulator::ldh(ac_int<32, false> addr)
{
    ac_int<16, true> result = 0;
    result.set_slc(8, this->ldb(addr+1));
    result.set_slc(0, this->ldb(addr));
    return result;
}

ac_int<32, true> Simulator::ldw(ac_int<32, false> addr)
{
    ac_int<32, true> result = 0;
    result.set_slc(24, this->ldb(addr+3));
    result.set_slc(16, this->ldb(addr+2));
    result.set_slc(8, this->ldb(addr+1));
    result.set_slc(0, this->ldb(addr));
    return result;
}

ac_int<32, true> Simulator::ldd(ac_int<32, false> addr)
{
    ac_int<32, true> result = 0;
    result.set_slc(56, this->ldb(addr+7));
    result.set_slc(48, this->ldb(addr+6));
    result.set_slc(40, this->ldb(addr+5));
    result.set_slc(32, this->ldb(addr+4));
    result.set_slc(24, this->ldb(addr+3));
    result.set_slc(16, this->ldb(addr+2));
    result.set_slc(8, this->ldb(addr+1));
    result.set_slc(0, this->ldb(addr));

    return result;
}





ac_int<32, true> Simulator::solveSyscall(ac_int<32, true> syscallId, ac_int<32, true> arg1, ac_int<32, true> arg2, ac_int<32, true> arg3, ac_int<32, true> arg4, bool &sys_status, unsigned int hartid)
{
    ac_int<32, true> result = 0;
    currentcore = hartid;
    switch (syscallId)
    {
    case SYS_exit:
        result = this->doExit(arg1, sys_status);
        break;
    case SYS_read:
        result = this->doRead(arg1, arg2, arg3);
        break;
    case SYS_write:
        result = this->doWrite(arg1, arg2, arg3);
        break;
    case SYS_brk:
        result = this->doSbrk(arg1);
        break;
    case SYS_open:
        result = this->doOpen(arg1, arg2, arg3);
        break;
    case SYS_openat:
        result = this->doOpenat(arg1, arg2, arg3, arg4);
        break;
    case SYS_lseek:
        result = this->doLseek(arg1, arg2, arg3);
        break;
    case SYS_close:
        result = this->doClose(arg1);
        break;
    case SYS_fstat:
        result = this->doFstat(arg1, arg2);
        break;
    case SYS_stat:
        result = this->doStat(arg1, arg2);
        break;
    case SYS_gettimeofday:
        result = this->doGettimeofday(arg1);
        break;
    case SYS_unlink:
        result = this->doUnlink(arg1);
        break;
    case SYS_exit_group:
        fprintf(stderr, "Syscall : SYS_exit_group\n");
        sys_status = 1;
        break;
    case SYS_getpid:
        fprintf(stderr, "Syscall : SYS_getpid\n");
        sys_status = 1;
        break;
    case SYS_kill:
        fprintf(stderr, "Syscall : SYS_kill\n");
        sys_status = 1;
        break;
    case SYS_link:
        fprintf(stderr, "Syscall : SYS_link\n");
        sys_status = 1;
        break;
    case SYS_mkdir:
        fprintf(stderr, "Syscall : SYS_mkdir\n");
        sys_status = 1;
        break;
    case SYS_chdir:
        fprintf(stderr, "Syscall : SYS_chdir\n");
        sys_status = 1;
        break;
    case SYS_getcwd:
        fprintf(stderr, "Syscall : SYS_getcwd\n");
        sys_status = 1;
        break;
    case SYS_lstat:
        fprintf(stderr, "Syscall : SYS_lstat\n");
        sys_status = 1;
        break;
    case SYS_fstatat:
        fprintf(stderr, "Syscall : SYS_fstatat\n");
        sys_status = 1;
        break;
    case SYS_access:
        fprintf(stderr, "Syscall : SYS_access\n");
        sys_status = 1;
        break;
    case SYS_faccessat:
        fprintf(stderr, "Syscall : SYS_faccessat\n");
        sys_status = 1;
        break;
    case SYS_pread:
        fprintf(stderr, "Syscall : SYS_pread\n");
        sys_status = 1;
        break;
    case SYS_pwrite:
        fprintf(stderr, "Syscall : SYS_pwrite\n");
        sys_status = 1;
        break;
    case SYS_uname:
        fprintf(stderr, "Syscall : SYS_uname\n");
        sys_status = 1;
        break;
    case SYS_getuid:
        fprintf(stderr, "Syscall : SYS_getuid\n");
        sys_status = 1;
        break;
    case SYS_geteuid:
        fprintf(stderr, "Syscall : SYS_geteuid\n");
        sys_status = 1;
        break;
    case SYS_getgid:
        fprintf(stderr, "Syscall : SYS_getgid\n");
        sys_status = 1;
        break;
    case SYS_getegid:
        fprintf(stderr, "Syscall : SYS_getegid\n");
        sys_status = 1;
        break;
    case SYS_mmap:
        fprintf(stderr, "Syscall : SYS_mmap\n");
        sys_status = 1;
        break;
    case SYS_munmap:
        fprintf(stderr, "Syscall : SYS_munmap\n");
        sys_status = 1;
        break;
    case SYS_mremap:
        fprintf(stderr, "Syscall : SYS_mremap\n");
        sys_status = 1;
        break;
    case SYS_time:
        fprintf(stderr, "Syscall : SYS_time\n");
        sys_status = 1;
        break;
    case SYS_getmainvars:
        fprintf(stderr, "Syscall : SYS_getmainvars\n");
        sys_status = 1;
        break;
    case SYS_rt_sigaction:
        fprintf(stderr, "Syscall : SYS_rt_sigaction\n");
        sys_status = 1;
        break;
    case SYS_writev:
        fprintf(stderr, "Syscall : SYS_writev\n");
        sys_status = 1;
        break;
    case SYS_times:
        fprintf(stderr, "Syscall : SYS_times\n");
        sys_status = 1;
        break;
    case SYS_fcntl:
        fprintf(stderr, "Syscall : SYS_fcntl\n");
        sys_status = 1;
        break;
    case SYS_getdents:
        fprintf(stderr, "Syscall : SYS_getdents\n");
        sys_status = 1;
        break;
    case SYS_dup:
        fprintf(stderr, "Syscall : SYS_dup\n");
        sys_status = 1;
        break;

    // Custom syscalls
    case SYS_threadstart:
        result = this->doThreadstart(arg1, arg2, arg3);
        break;
    case SYS_nbcore:
        dbgsys("SYS_nbcore");
        result = doNbcore();
        break;


    default:
        fprintf(stderr, "Syscall : Unknown system call, %d (%x) with arguments :\n", syscallId.to_int(), syscallId.to_int());
        fprintf(stderr, "%d (%x)\n%d (%x)\n%d (%x)\n%d (%x)\n", arg1.to_int(), arg1.to_int(), arg2.to_int(), arg2.to_int(),
                arg3.to_int(), arg3.to_int(), arg4.to_int(), arg4.to_int());
        sys_status = 1;
        break;
    }

    return result;
}

ac_int<32, true> Simulator::doThreadstart(ac_int<32, false> tid, ac_int<32, false> fnptr, ac_int<32, false> fnargs)
{
    dbgsys("SYS_threadstart id %d fn @%06x args @%06x", tid.to_int(), fnptr.to_int(), fnargs.to_int());
    dbgassert(tid < doNbcore(), "Cannot start thread %d because core %d does not exist\n", tid.to_int(), tid.to_int());
    dbgassert(core[tid]->ctrl.sleep == true, "Core %d is already running\n", tid.to_int());
    core[tid]->ctrl.sleep = false;
    core[tid]->pc = fnptr;
    for(int i(0); i < 32; ++i)      // copy registers
        core[tid]->REG[i] = core[currentcore]->REG[i];
    core[tid]->REG[1] = exit;       // set proper return address for thread
    core[tid]->REG[2] = core[currentcore]->REG[2]-0x1000; //we must move it to different address, otherwise stacks are overlapping
    core[tid]->REG[10] = fnargs;    // function arguments?
    // copy stack as well
    for(int ad(core[currentcore]->REG[2]-0x800); ad < core[currentcore]->REG[2]+0x800; ++ad)
    {
        dm[(ad-0x1000)/4] = this->ldw(ad);
    }

    return 0;
}

ac_int<32, true> Simulator::doNbcore()
{
    return 2;
}

ac_int<32, true> Simulator::doExit(ac_int<32, false> retcode, bool& sys_status)
{
    if(currentcore == 0)
        sys_status = 1;

    if(retcode)
        dbgsys("SYS_exit with code \033[1;31m%d\033[0m", retcode.to_int());
    else
        dbgsys("SYS_exit with code \033[32m0\033[0m");

    return 0;
}

ac_int<32, true> Simulator::doRead(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
    dbgsys("SYS_read %d bytes from file %d", size.to_int(), file.to_int());
    char* localBuffer = new char[size.to_int()];
    ac_int<32, true> result;

    if(file == 0 && input)
        result = read(input->_fileno, localBuffer, size);
    else
        result = read(file, localBuffer, size);

    for (int i(0); i < result; i++)
    {
        this->stb(bufferAddr + i, localBuffer[i]);
        //fprintf(stderr, "%02x ", localBuffer[i]&0xFF);
    }
    //fprintf(stderr, "\n\n");

    delete[] localBuffer;
    return result;
}


ac_int<32, true> Simulator::doWrite(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
    //dbgsys("SYS_write %d bytes to file %d", size.to_int(), file.to_int());
    char* localBuffer = new char[size.to_int()];
    for (int i=0; i<size; i++)
        localBuffer[i] = this->ldb(bufferAddr + i);

    ac_int<32, true> result = 0;
    if(file == 1 && output)  // 3 is the first available file descriptor
    {
        fflush(stdout);
        result = write(output->_fileno, localBuffer, size);
    }
    else
    {
        if(file == 1)
            fflush(stdout); // prevent mixed output
        result = write(file, localBuffer, size);
    }
    delete[] localBuffer;
    return result;
}

ac_int<32, true> Simulator::doFstat(ac_int<32, false> file, ac_int<32, false> stataddr)
{
    dbgsys("SYS_fstat on file %d", file.to_int());
    ac_int<32, true> result = 0;
    struct stat filestat = {0};     // for stdout, its easier to compare debug trace when syscalls gives same results

    if(file != 1)
        result = fstat(file, &filestat);

    /*struct  kernel_stat
    {
      unsigned long long st_dev;
      unsigned long long st_ino;
      unsigned int st_mode;
      unsigned int st_nlink;
      unsigned int st_uid;
      unsigned int st_gid;
      unsigned long long st_rdev;
      unsigned long long __pad1;
      long long st_size;
      int st_blksize;
      int __pad2;
      long long st_blocks;
      struct timespec st_atim;
      struct timespec st_mtim;
      struct timespec st_ctim;
      int __glibc_reserved[2];
    };*/
    std(stataddr    , filestat.st_dev         );  // unsigned long long
    std(stataddr+8  , filestat.st_ino         );  // unsigned long long
    stw(stataddr+16 , filestat.st_mode        );  // unsigned int
    stw(stataddr+20 , filestat.st_nlink       );  // unsigned int
    stw(stataddr+24 , filestat.st_uid         );  // unsigned int
    stw(stataddr+28 , filestat.st_gid         );  // unsigned int
    std(stataddr+32 , filestat.st_rdev        );  // unsigned long long
    std(stataddr+40 , filestat.__pad0         );  // unsigned long long
    std(stataddr+48 , filestat.st_size        );  // long long
    stw(stataddr+56 , filestat.st_blksize     );  // int
    stw(stataddr+60 , filestat.__pad0         );  // int
    std(stataddr+64 , filestat.st_blocks      );  // long long
    stw(stataddr+72 , filestat.st_atim.tv_sec );  // long
    stw(stataddr+76 , filestat.st_atim.tv_nsec);  // long
    stw(stataddr+80 , filestat.st_mtim.tv_sec );  // long
    stw(stataddr+84 , filestat.st_mtim.tv_nsec);  // long
    stw(stataddr+88 , filestat.st_ctim.tv_sec );  // long
    stw(stataddr+92 , filestat.st_ctim.tv_nsec);  // long
    stw(stataddr+96 , filestat.__pad0         );  // long
    stw(stataddr+100, filestat.__pad0         );  // long

    /*fprintf(stderr, "st_dev         : %lld\n", filestat.st_dev         );  // unsigned long long
    fprintf(stderr, "st_ino         : %lld\n", filestat.st_ino         );  // unsigned long long
    fprintf(stderr, "st_mode        : %o\n", filestat.st_mode        );  // unsigned int
    fprintf(stderr, "st_nlink       : %d\n", filestat.st_nlink       );  // unsigned int
    fprintf(stderr, "st_uid         : %d\n", filestat.st_uid         );  // unsigned int
    fprintf(stderr, "st_gid         : %d\n", filestat.st_gid         );  // unsigned int
    fprintf(stderr, "st_rdev        : %lld\n", filestat.st_rdev        );  // unsigned long long
    fprintf(stderr, "st_size        : %lld\n", filestat.st_size        );  // long long
    fprintf(stderr, "st_blksize     : %d\n", filestat.st_blksize     );  // int
    fprintf(stderr, "st_blocks      : %lld\n", filestat.st_blocks      );  // long long
    fprintf(stderr, "st_atim.sec    : %d\n", filestat.st_atim.tv_sec );  // long
    fprintf(stderr, "st_atim.nsec   : %d\n", filestat.st_atim.tv_nsec);  // long
    fprintf(stderr, "st_mtim.sec    : %d\n", filestat.st_mtim.tv_sec );  // long
    fprintf(stderr, "st_mtim.nsec   : %d\n", filestat.st_mtim.tv_nsec);  // long
    fprintf(stderr, "st_ctim.sec    : %d\n", filestat.st_ctim.tv_sec );  // long
    fprintf(stderr, "st_ctim.nsec   : %d\n", filestat.st_ctim.tv_nsec);  // long*/

    return result;
}

ac_int<32, true> Simulator::doOpen(ac_int<32, false> path, ac_int<32, false> flags, ac_int<32, false> mode)
{
    int oneStringElement = this->ldb(path);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = this->ldb(path+index);
    }

    int pathSize = index+1;

    char* localPath = new char[pathSize+1];
    for (int i=0; i<pathSize; i++)
        localPath[i] = this->ldb(path + i);
    localPath[pathSize] = '\0';

    // convert riscv flags to unix flags
    int riscvflags = flags;
    std::string str;
    if(riscvflags & SYS_O_WRONLY)
        str += "WRONLY, ";
    else if(riscvflags & SYS_O_RDWR)
        str += "RDWR, ";
    else
        str += "RDONLY, ";
    int unixflags = riscvflags&3;    // O_RDONLY, O_WRITE, O_RDWR are the same
    riscvflags ^= unixflags;
    if(riscvflags & SYS_O_APPEND)
    {
        unixflags |= O_APPEND;
        riscvflags ^= SYS_O_APPEND;
        str += "APPEND, ";
    }
    if(riscvflags & SYS_O_CREAT)
    {
        unixflags |= O_CREAT;
        riscvflags ^= SYS_O_CREAT;
        str += "CREAT, ";
    }
    if(riscvflags & SYS_O_TRUNC)
    {
        unixflags |= O_TRUNC;
        riscvflags ^= SYS_O_TRUNC;
        str += "TRUNC, ";
    }
    if(riscvflags & SYS_O_EXCL)
    {
        unixflags |= O_EXCL;
        riscvflags ^= SYS_O_EXCL;
        str += "EXCL, ";
    }
    if(riscvflags & SYS_O_SYNC)
    {
        unixflags |= O_SYNC;
        riscvflags ^= SYS_O_SYNC;
        str += "SYNC, ";
    }
    if(riscvflags & SYS_O_NONBLOCK)
    {
        unixflags |= O_NONBLOCK;
        riscvflags ^= SYS_O_NONBLOCK;
        str += "NONBLOCK, ";
    }
    if(riscvflags & SYS_O_NOCTTY)
    {
        unixflags |= O_NOCTTY;
        riscvflags ^= SYS_O_NOCTTY;
        str += "NOCTTY";
    }
    dbgsys("SYS_open on file %s with flags %s (%x) and mode %o", localPath, str.c_str(), unixflags, mode.to_int());
    dbgassert(riscvflags == 0, "Some flags were not translated!\n");
    int result  = open(localPath, unixflags, mode.to_int());

    delete[] localPath;
    return result;

}

ac_int<32, true> Simulator::doOpenat(ac_int<32, false> dir, ac_int<32, false> path, ac_int<32, false> flags, ac_int<32, false> mode)
{
    fprintf(stderr, "Syscall : SYS_openat not implemented yet...\n");
    std::exit(-1);
}

ac_int<32, true> Simulator::doClose(ac_int<32, false> file)
{
    dbgsys("SYS_close on file %d", file.to_int());
    if(file > 2)    // don't close simulator's stdin, stdout & stderr
    {
        return close(file);
    }

    return 0;
}

ac_int<32, true> Simulator::doLseek(ac_int<32, false> file, ac_int<32, false> ptr, ac_int<32, false> dir)
{
    int result = lseek(file, ptr, dir);
    dbgsys("SYS_lseek on file %d    ptr %d      dir %d", file.to_int(), ptr.to_int(), dir.to_int());
    return result;
}

ac_int<32, true> Simulator::doStat(ac_int<32, false> filename, ac_int<32, false> stataddr)
{
    int oneStringElement = this->ldb(filename);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = this->ldb(filename+index);
    }

    int pathSize = index+1;

    char* localPath = new char[pathSize+1];
    for (int i=0; i<pathSize; i++)
        localPath[i] = this->ldb(filename + i);
    localPath[pathSize] = '\0';
    dbgsys("SYS_stat on %s", localPath);

    struct stat filestat;
    int result = stat(localPath, &filestat);

    std(stataddr    , filestat.st_dev         );  // unsigned long long
    std(stataddr+8  , filestat.st_ino         );  // unsigned long long
    stw(stataddr+16 , filestat.st_mode        );  // unsigned int
    stw(stataddr+20 , filestat.st_nlink       );  // unsigned int
    stw(stataddr+24 , filestat.st_uid         );  // unsigned int
    stw(stataddr+28 , filestat.st_gid         );  // unsigned int
    std(stataddr+32 , filestat.st_rdev        );  // unsigned long long
    std(stataddr+40 , filestat.__pad0         );  // unsigned long long
    std(stataddr+48 , filestat.st_size        );  // long long
    stw(stataddr+56 , filestat.st_blksize     );  // int
    stw(stataddr+60 , filestat.__pad0         );  // int
    std(stataddr+64 , filestat.st_blocks      );  // long long
    stw(stataddr+72 , filestat.st_atim.tv_sec );  // long
    stw(stataddr+76 , filestat.st_atim.tv_nsec);  // long
    stw(stataddr+80 , filestat.st_mtim.tv_sec );  // long
    stw(stataddr+84 , filestat.st_mtim.tv_nsec);  // long
    stw(stataddr+88 , filestat.st_ctim.tv_sec );  // long
    stw(stataddr+92 , filestat.st_ctim.tv_nsec);  // long
    stw(stataddr+96 , filestat.__pad0         );  // long
    stw(stataddr+100, filestat.__pad0         );  // long

    delete[] localPath;
    return result;
}

ac_int<32, true> Simulator::doSbrk(ac_int<32, false> value)
{
    dbgsys("SYS_brk  heap @0x%x -> @0x%x (%+d)", heapAddress, value?value.to_int():heapAddress, value?value.to_int()-heapAddress:0);

    ac_int<32, true> result;
    if (value == 0)
    {
        result = heapAddress;
    }
    else
    {
        heapAddress = value;
        result = value;
    }

    dbgassert(core[0]->REG[2].to_uint() > heapAddress, "Stack and heap overlaps (%08x > %08x)!!\n", core[0]->REG[2].to_uint(), heapAddress);
    
    return result;
}

ac_int<32, true> Simulator::doGettimeofday(ac_int<32, false> timeValPtr)
{
    dbgsys("SYS_gettimeofday");
    struct timeval oneTimeVal;
    int result = gettimeofday(&oneTimeVal, NULL);

    this->stw(timeValPtr, oneTimeVal.tv_sec);
    this->stw(timeValPtr+4, oneTimeVal.tv_usec);

    return result;
}

ac_int<32, true> Simulator::doUnlink(ac_int<32, false> path)
{
    int oneStringElement = this->ldb(path);
    int index = 0;
    while (oneStringElement != '\0')
    {
        index++;
        oneStringElement = this->ldb(path+index);
    }

    int pathSize = index+1;

    char* localPath = new char[pathSize+1];
    for (int i=0; i<pathSize; i++)
        localPath[i] = this->ldb(path + i);
    localPath[pathSize] = '\0';
    dbgsys("SYS_unlink on file %s", localPath);

    int result = unlink(localPath);

    delete[] localPath;
    return result;
}
