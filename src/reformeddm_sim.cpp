#include "mc_scverify.h"
#include "elfFile.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>

#include <iostream>
#include <bitset>
#include <string.h>

#include "portability.h"
#include "cache.h"
#include "core.h"
#include "simulator.h"


using namespace std;

CCS_MAIN(int argc, char** argv)
{
#ifndef nocache
    printf("Parameters : %5s   %8s   %13s   %4s   %6s   %13s    %13s\n", "Size", "Blocksize", "Associativity", "Sets", "Policy", "icontrolwidth", "dcontrolwidth");
    printf("Parameters : %5d   %8d   %13d   %4d   %6d   %13d    %13d\n", Size, 4*Blocksize, Associativity, Sets, Policy, ICacheControlWidth, DCacheControlWidth);
#endif

    const char* binaryFile = 0;
    const char* inputFile = 0;
    const char* outputFile = 0;
    int argstart = 0;
    char **benchargv = 0;
    int benchargc = 1;
    for(int i = 1; i < argc; ++i)
    {
        if(strcmp("-i", argv[i]) == 0)
        {
            inputFile = argv[i+1];
        }
        else if(strcmp("-o", argv[i]) == 0)
        {
            outputFile = argv[i+1];
        }
        else if(strcmp("-f", argv[i]) == 0)
        {
            binaryFile = argv[i+1];
        }
        else if(strcmp("--", argv[i]) == 0)
        {
            argstart = i+1;
            benchargc = argc - i;
            benchargv = new char*[benchargc];
            break;
        }
    }
    if(benchargv == 0)
        benchargv = new char*[benchargc];

    if(binaryFile == 0)
#ifdef __HLS__
        binaryFile = "matmul.riscv32";
#else
        binaryFile = "benchmarks/build/matmul_int_4.riscv32";
#endif

    //fprintf(stderr, "%d bench arguments\n", benchargc);
    benchargv[0] = new char[strlen(binaryFile)+1];
    strcpy(benchargv[0], binaryFile);
    //fprintf(stderr, "%s\n", benchargv[0]);
    for(int i(1); i < benchargc; ++i)
    {
        benchargv[i] = new char[strlen(argv[argstart + i - 1])+1];
        strcpy(benchargv[i], argv[argstart + i - 1]);
        //fprintf(stderr, "%s\n", benchargv[i]);
    }

    fprintf(stderr, "Simulating %s\n", binaryFile);
    std::cout << "Simulating " << binaryFile << std::endl;

    Simulator sim(binaryFile, inputFile, outputFile, benchargc, benchargv);

    unsigned int* dm = new unsigned int[DRAM_SIZE];
    unsigned int* im = new unsigned int[DRAM_SIZE];
    for(int i = 0; i < DRAM_SIZE; i++)
    {
        dm[i] = sim.getDataMemory()[i];
        im[i] = sim.getInstructionMemory()[i];
    }

    sim.setDM(dm);
    sim.setIM(im);   

    unsigned int* cim = new unsigned int[Sets*Blocksize*Associativity];
    unsigned int* cdm = new unsigned int[Sets*Blocksize*Associativity];

    ac_int<IWidth, false>* memictrl = new ac_int<IWidth, false>[Sets];
    ac_int<DWidth, false>* memdctrl = new ac_int<DWidth, false>[Sets];

    // zero the control (although only the valid bit should be zeroed, rest is don't care)
    for(int i(0); i < Sets; ++i)
    {
        memictrl[i] = 0;
        memdctrl[i] = 0;
    }

    sim.setCore(0, memdctrl, (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(cdm)));

    /*unsigned int (&cim)[Sets][Blocksize][Associativity] = (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(cacheim));
    unsigned int (&cdm)[Sets][Blocksize][Associativity] = (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(cachedm));*/

    coredebug("instruction memory :\n");
    for(int i = 0; i < DRAM_SIZE; i++)
    {
        if(im[i])
            coredebug("%06x : %08x (%d)\n", 4*i, im[i], im[i]);
    }
    coredebug("data memory :\n");
    for(int i = 0; i < DRAM_SIZE; i++)
    {
        for(int j(0); j < 4; ++j)
        {
            if(dm[i] & (0xFF << (8*j)))
            {
                coredebug("%06x : %02x (%d)\n", 4*i+j, (dm[i] & (0xFF << (8*j))) >> (8*j), (dm[i] & (0xFF << (8*j))) >> (8*j));
            }
        }
    }
    coredebug("end of preambule\n");

    bool exit = false;
    //core.pc = sim.getPC();
    while(!exit)
    {
        CCS_DESIGN(doStep(sim.getPC(), exit,
/* main memories */       im, dm,
/** cache memories **/    (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(cim)), (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(cdm)),
/* control memories */    memictrl, memdctrl
                  #ifndef __HLS__
                      , &sim
                  #endif
                  ));
    }

    sim.writeBack();

#ifndef __HLS__
    printf("Successfully executed %lld instructions in %lld cycles\n", sim.getCore()->csrs.minstret.to_int64(), sim.getCore()->csrs.mcycle.to_int64());
    fprintf(stderr, "Successfully executed %lld instructions in %lld cycles\n", sim.getCore()->csrs.minstret.to_int64(), sim.getCore()->csrs.mcycle.to_int64());

    coredebug("memory : \n");
    for(int i = 0; i < DRAM_SIZE; i++)
    {
        for(int j(0); j < 4; ++j)
        {
            if(dm[i] & (0xFF << (8*j)))
            {
                coredebug("%06x : %02x (%d)\n", 4*i+j, (dm[i] & (0xFF << (8*j))) >> (8*j), (dm[i] & (0xFF << (8*j))) >> (8*j));
            }
        }
    }

#ifndef nocache

    printf("Cache Parameters : %5s   %8s   %13s   %4s   %6s   %13s    %13s\n", "Size", "Blocksize", "Associativity", "Sets", "Policy", "icontrolwidth", "dcontrolwidth");
    printf("                   %5d   %8d   %13d   %4d   %6d   %13d    %13d\n", Size, 4*Blocksize, Associativity, Sets, Policy, ICacheControlWidth, DCacheControlWidth);
    printf("Cache Statistics : \n");
    printf("%15s   %17s %17s\n", "", "Instruction Cache", "Data Cache");
    printf("%15s : %17lld %17lld\n", "Miss", sim.icachedata.miss, sim.dcachedata.miss);
    printf("%15s : %17lld %17lld\n", "Hit", sim.icachedata.hit, sim.dcachedata.hit);
    printf("%15s : %17lld %17lld\n", "Cachemem Read", sim.icachedata.cachememread, sim.dcachedata.cachememread);
    printf("%15s : %17lld %17lld\n", "Cachemem Write", sim.icachedata.cachememwrite, sim.dcachedata.cachememwrite);
    printf("%15s : %17lld %17lld\n", "Ctrlmem Read", sim.icachedata.ctrlmemread, sim.dcachedata.ctrlmemread);
    printf("%15s : %17lld %17lld\n", "Ctrlmem Write", sim.icachedata.ctrlmemwrite, sim.dcachedata.ctrlmemwrite);
    printf("%15s : %17lld %17lld\n", "Mainmem Read", sim.icachedata.mainmemread, sim.dcachedata.mainmemread);
    printf("%15s : %17lld %17lld\n", "Mainmem Write", sim.icachedata.mainmemwrite, sim.dcachedata.mainmemwrite);

#endif

#else
    printf("memory : \n");
    for(int i = 0; i < DRAM_SIZE; i++)
    {
        for(int j(0); j < 4; ++j)
        {
            if(dm[i] & (0xFF << (8*j)))
            {
                printf("%06x : %02x (%d)\n", 4*i+j, (dm[i] & (0xFF << (8*j))) >> (8*j), (dm[i] & (0xFF << (8*j))) >> (8*j));
            }
        }
    }
#endif

    for(int i = 0; i < benchargc; ++i)
        delete[] benchargv[i];
    delete[] benchargv;
    delete[] dm;
    delete[] im;
    delete[] cim;
    delete[] cdm;
    delete[] memictrl;
    delete[] memdctrl;
    CCS_RETURN(0);
}
