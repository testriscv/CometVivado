#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "simulator.h"

Simulator::Simulator(const char* binaryFile, const char* inputFile, const char* outputFile, int benchargc, char **benchargv)
{
    ins_memory = (ac_int<32, true> *)malloc(DRAM_SIZE * sizeof(ac_int<32, true>));
    data_memory = (ac_int<32, true> *)malloc(DRAM_SIZE * sizeof(ac_int<32, true>));
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
    for (unsigned int sectionCounter = 0; sectionCounter < elfFile.sectionTable->size(); sectionCounter++)
    {
        ElfSection *oneSection = elfFile.sectionTable->at(sectionCounter);
        if(oneSection->address != 0 && strncmp(oneSection->getName().c_str(), ".text", 5))
        {
            //If the address is not null we place its content into memory
            unsigned char* sectionContent = oneSection->getSectionCode();
            for (unsigned int byteNumber = 0; byteNumber<oneSection->size; byteNumber++)
            {
                counter++;
                setDataMemory(oneSection->address + byteNumber, sectionContent[byteNumber]);
            }
            coredebug("filling data from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }

        if(!strncmp(oneSection->getName().c_str(), ".text", 5))
        {
            unsigned char* sectionContent = oneSection->getSectionCode();
            for (unsigned int byteNumber = 0; byteNumber < oneSection->size; byteNumber++)
            {
                setInstructionMemory((oneSection->address + byteNumber), sectionContent[byteNumber]);
            }
            coredebug("filling instruction from %06x to %06x\n", oneSection->address, oneSection->address + oneSection->size -1);
        }
    }

    for (int oneSymbol = 0; oneSymbol < elfFile.symbols->size(); oneSymbol++)
    {
        ElfSymbol *symbol = elfFile.symbols->at(oneSymbol);
        const char* name = (const char*) &(elfFile.sectionTable->at(elfFile.indexOfSymbolNameSection)->getSectionCode()[symbol->name]);
        if (strcmp(name, "_start") == 0)
        {
            fprintf(stderr, "%s     @%06x\n", name, symbol->offset);
            setPC(symbol->offset);
        }
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
    free(ins_memory);
    free(data_memory);

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
        exit(-1);
    }
    if(data_memorymap.size() / 4 > DRAM_SIZE)
    {
        printf("Error! Data memory size exceeded");
        exit(-1);
    }

    //fill instruction memory
    for(std::map<ac_int<32, false>, ac_int<8, false> >::iterator it = ins_memorymap.begin(); it!=ins_memorymap.end(); ++it)
    {
        ins_memory[(it->first/4) % DRAM_SIZE].set_slc(((it->first)%4)*8,it->second);
        //gdebug("@%06x    @%06x    %d    %02x\n", it->first, (it->first/4) % DRAM_SIZE, ((it->first)%4)*8, it->second);
    }

    //fill data memory
    for(std::map<ac_int<32, false>, ac_int<8, false> >::iterator it = data_memorymap.begin(); it!=data_memorymap.end(); ++it)
    {
        //data_memory.set_byte((it->first/4)%DRAM_SIZE,it->second,it->first%4);
        data_memory[(it->first/4)%DRAM_SIZE].set_slc(((it->first%DRAM_SIZE)%4)*8,it->second);
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

void Simulator::setCore(ac_int<32, true> *r, DCacheControl* ctrl, unsigned int cachedata[Sets][Blocksize][Associativity])
{
    reg = r;
    dctrl = ctrl;
    ddata = (unsigned int*)cachedata;
}

ac_int<32, true>* Simulator::getInstructionMemory() const
{
    return ins_memory;
}

ac_int<32, true>* Simulator::getDataMemory() const
{
    return data_memory;
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
        if(dctrl->tag[i][j] == getTag(addr))
        {
            ac_int<32, false> mem = ddata[i*Blocksize*Associativity + (int)getOffset(addr)*Associativity + j];
            formatwrite(addr, 0, mem, value);
            ddata[i*Blocksize*Associativity + (int)getOffset(addr)*Associativity + j] = mem;
            dctrl->dirty[i][j] = true;      // mark as dirty because we wrote it
            //fprintf(stderr, "data @%06x (%06x) is in cache\n", addr.to_int(), dctrl->tag[i][j].to_int());
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

void Simulator::std(ac_int<32, false> addr, ac_int<32, true> value)
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
        if(dctrl->tag[i][j] == getTag(addr))
        {
            ac_int<32, false> mem = ddata[i*Blocksize*Associativity + (int)getOffset(addr)*Associativity + j];
            formatread(addr, 0, 0, mem);
            //fprintf(stderr, "data @%06x (%06x) is in cache\n", addr.to_int(), dctrl->tag[i][j].to_int());
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





ac_int<32, true> Simulator::solveSyscall(ac_int<32, true> syscallId, ac_int<32, true> arg1, ac_int<32, true> arg2, ac_int<32, true> arg3, ac_int<32, true> arg4, bool &sys_status)
{
    ac_int<32, true> result = 0;
    switch (syscallId)
    {
    case SYS_exit:
        sys_status = 1; //Currently we break on ECALL
        fprintf(stderr, "Syscall : SYS_exit\n");
        break;
    case SYS_read:
        fprintf(stderr, "Syscall : SYS_read\n");
        result = this->doRead(arg1, arg2, arg3);
        break;
    case SYS_write:
        fprintf(stderr, "Syscall : SYS_write\n");
        result = this->doWrite(arg1, arg2, arg3);
        break;
    case SYS_brk:
        fprintf(stderr, "Syscall : SYS_brk\n");
        result = this->doSbrk(arg1);
        break;
    case SYS_open:
        fprintf(stderr, "Syscall : SYS_open\n");
        result = this->doOpen(arg1, arg2, arg3);
        break;
    case SYS_openat:
        fprintf(stderr, "Syscall : SYS_openat\n");
        result = this->doOpenat(arg1, arg2, arg3, arg4);
        break;
    case SYS_lseek:
        fprintf(stderr, "Syscall : SYS_lseek\n");
        result = this->doLseek(arg1, arg2, arg3);
        break;
    case SYS_close:
        fprintf(stderr, "Syscall : SYS_close\n");
        result = this->doClose(arg1);
        break;
    case SYS_fstat:
        fprintf(stderr, "Syscall : SYS_fstat\n");
        result = 0;
        break;
    case SYS_stat:
        fprintf(stderr, "Syscall : SYS_stat\n");
        result = this->doStat(arg1, arg2);
        break;
    case SYS_gettimeofday:
        fprintf(stderr, "Syscall : SYS_gettimeofday\n");
        result = this->doGettimeofday(arg1);
        break;
    case SYS_unlink:
        fprintf(stderr, "Syscall : SYS_unlink\n");
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
    default:
        fprintf(stderr, "Syscall : Unknown system call, %d\n", syscallId.to_int());
        sys_status = 1;
        break;
    }

    return result;
}

ac_int<32, false> Simulator::doRead(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
    //fprintf(stderr, "Doing read on file %x\n", file);

    int localSize = size.slc<32>(0);
    char* localBuffer = (char*) malloc(localSize*sizeof(char));
    ac_int<32, false> result;

    if (file == 0)
    {
        if(input)
        {
            fprintf(stderr, "Reading %d bytes on input file\n", size.to_int());
            result = fread(localBuffer, 1, size, input);
        }
        else
        {
            fprintf(stderr, "Reading %d bytes on stdin\n", size.to_int());
            result = fread(localBuffer, 1, size, stdin);
        }
    }
    else
    {
        FILE* localFile = this->fileMap[file.slc<16>(0)];
        fprintf(stderr, "Reading %d bytes on localfile %lld\n", size.to_int(), (long long)localFile);
        result = fread(localBuffer, 1, size, localFile);
        if (localFile == 0)
            return -1;
    }

    for (int i(0); i < result; i++)
    {
        this->stb(bufferAddr + i, localBuffer[i]);
        //fprintf(stderr, "%02x ", localBuffer[i]&0xFF);
    }
    //fprintf(stderr, "\n\n");

    free(localBuffer);
    return result;
}


ac_int<32, false> Simulator::doWrite(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
    int localSize = size.slc<32>(0);
    char* localBuffer = (char*) malloc(localSize*sizeof(char)+1);
    for (int i=0; i<size; i++)
        localBuffer[i] = this->ldb(bufferAddr + i);
    localBuffer[size] = 0;

    ac_int<32, false> result = 0;
    if (file < 5)
    {
        if (output)
        {
            fprintf(stderr, "Writing %d bytes to output file\n", size.to_int());
            result = fwrite(localBuffer, 1, size, output);
        }
        else
        {
            fprintf(stderr, "Writing %d bytes to stdout\n", size.to_int());
            result = fwrite(localBuffer, 1, size, stderr);
            //fprintf(stderr, "Write %d bytes : %s\nResult : %d\n", size.to_int(), localBuffer, result.to_int());
        }

    }
    else
    {
        FILE* localFile = this->fileMap[file.slc<16>(0)];
        if (localFile == 0)
            result = -1;
        else
            result = fwrite(localBuffer, 1, size, localFile);
        fprintf(stderr, "Writing %d bytes to localfile %lld\n", size.to_int(), (long long)localFile);
    }

    free(localBuffer);
    return result;
}


ac_int<32, false> Simulator::doOpen(ac_int<32, false> path, ac_int<32, false> flags, ac_int<32, false> mode)
{
    int oneStringElement = this->ldb(path);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = this->ldb(path+index);
    }

    int pathSize = index+1;

    char* localPath = (char*) malloc(pathSize*sizeof(char)+1);
    for (int i=0; i<pathSize; i++)
        localPath[i] = this->ldb(path + i);

    const char* localMode;
    if(flags == O_RDONLY)
        localMode = "r";
    else if(flags & O_WRONLY)
    {
        if(flags & O_TRUNC)
            localMode = "w";
        else if(flags & O_APPEND)
            localMode = "a";
        else
            localMode = "w";
    }
    else if(flags & O_RDWR)
    {
        if(flags & O_TRUNC)
            localMode = "w+";
        else if(flags & O_APPEND)
            localMode = "a+";
        else
            localMode = "r+";
    }
    else
    {
        fprintf(stderr, "Trying to open files with unknown flags... %o\n", flags.to_int());
        exit(-1);
    }

    FILE* test = fopen(localPath, localMode);
    unsigned long long result = (unsigned long long) test;
    ac_int<32, true> result_ac = result;
    if(test == 0)
    {
        fprintf(stderr, "Could not open %s with mode %s (%o), reason : ", localPath, localMode, flags.to_int());
        perror("");
        exit(-1);
    }
    else
    {
        fprintf(stderr, "Opening %s with mode %s as file %lld\n", localPath, localMode, (long long)test);
    }

    //For some reasons, newlib only store last 16 bits of this pointer, we will then compute a hash and return that.
    //The real pointer is stored here in a hashmap

    ac_int<32, true> returnedResult = 0;
    returnedResult.set_slc(0, result_ac.slc<15>(0) ^ result_ac.slc<15>(16));
    returnedResult[15] = 0;

    this->fileMap[returnedResult.slc<16>(0)] = test;


    free(localPath);
    return returnedResult;

}

ac_int<32, false> Simulator::doOpenat(ac_int<32, false> dir, ac_int<32, false> path, ac_int<32, false> flags, ac_int<32, false> mode)
{
    fprintf(stderr, "Syscall openat not implemented yet...\n");
    exit(-1);
}

ac_int<32, false> Simulator::doClose(ac_int<32, false> file)
{
    if (file > 2 )
    {
        FILE* localFile = this->fileMap[file.slc<16>(0)];
        int result = fclose(localFile);
        fprintf(stderr, "Closing localfile %lld\n", (long long)localFile);
        return result;
    }
    else
    {
        fprintf(stderr, "Closing %d\n", file.to_int());
        return 0;
    }
}

ac_int<32, true> Simulator::doLseek(ac_int<32, false> file, ac_int<32, false> ptr, ac_int<32, false> dir)
{
    if (file>2)
    {
        FILE* localFile = this->fileMap[file.slc<16>(0)];
        if (localFile == 0)
            return -1;
        int result = fseek(localFile, ptr, dir);
        return result;
    }
    else
        return 0;
}

ac_int<32, false> Simulator::doStat(ac_int<32, false> filename, ac_int<32, false> ptr)
{
    int oneStringElement = this->ldb(filename);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = this->ldb(filename+index);
    }

    int pathSize = index+1;

    char* localPath = (char*) malloc(pathSize*sizeof(char));
    for (int i=0; i<pathSize; i++)
        localPath[i] = this->ldb(filename + i);

    struct stat fileStat;
    int result = stat(localPath, &fileStat);

    //We copy the result in simulator memory
    for (int oneChar = 0; oneChar<sizeof(struct stat); oneChar++)
        this->stb(ptr+oneChar, ((char*)(&stat))[oneChar]);

    free(localPath);
    return result;
}

ac_int<32, false> Simulator::doSbrk(ac_int<32, false> value)
{
    fprintf(stderr, "sbrk : %d -> %d (%d)\n", heapAddress, value.to_int(), abs(value.to_int()-(int)heapAddress));
    ac_int<32, false> result;
    if (value == 0)
    {
        result = heapAddress;
    }
    else
    {
        this->heapAddress = value;
        result = value;
    }

    if(reg[2] < heapAddress)
    {
        dbgassert(reg[2] > heapAddress, "Stack and heap overlaps !!\n");
    }
    return result;
}

ac_int<32, false> Simulator::doGettimeofday(ac_int<32, false> timeValPtr)
{
    timeval* oneTimeVal;
    struct timezone* oneTimeZone;
    int result = gettimeofday(oneTimeVal, oneTimeZone);

//    this->std(timeValPtr, oneTimeVal->tv_sec);
//    this->std(timeValPtr+8, oneTimeVal->tv_usec);

    return result;
}

ac_int<32, false> Simulator::doUnlink(ac_int<32, false> path)
{
    int oneStringElement = this->ldb(path);
    int index = 0;
    while (oneStringElement != 0)
    {
        index++;
        oneStringElement = this->ldb(path+index);
    }

    int pathSize = index+1;

    char* localPath = (char*) malloc(pathSize*sizeof(char));
    for (int i=0; i<pathSize; i++)
        localPath[i] = this->ldb(path + i);


    int result = unlink(localPath);

    free(localPath);

    return result;
}
