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
#include "cache.h"
#include "core.h"
#include "portability.h"
#include "simulator.h"

#define ptrtocache(mem) (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(mem))

void createCacheMemories(ac_int<IWidth, false>** memictrl, unsigned int** cim,
                         ac_int<DWidth, false>** memdctrl, unsigned int** cdm)
{
    *cim = new unsigned int[Sets*Blocksize*Associativity];
    *cdm = new unsigned int[Sets*Blocksize*Associativity];

    *memictrl = new ac_int<IWidth, false>[Sets];
    *memdctrl = new ac_int<DWidth, false>[Sets];
}

void deleteCacheMemories(ac_int<IWidth, false>** memictrl, unsigned int** cim,
                         ac_int<DWidth, false>** memdctrl, unsigned int** cdm)
{
    delete[] *cim;
    delete[] *cdm;
    delete[] *memictrl;
    delete[] *memdctrl;

    *memictrl = 0;
    *memdctrl = 0;
    *cim = 0;
    *cdm = 0;
}

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
        binaryFile = "matmul.riscv";
#else
        binaryFile = "benchmarks/build/matmul_int_4.riscv";
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

    unsigned int** cim = new unsigned int*[sim.doNbcore()];
    unsigned int** cdm = new unsigned int*[sim.doNbcore()];
    ac_int<IWidth, false>** memictrl = new ac_int<IWidth, false>*[sim.doNbcore()];
    ac_int<DWidth, false>** memdctrl = new ac_int<DWidth, false>*[sim.doNbcore()];

    for(int i(0); i < sim.doNbcore(); ++i)
    {
        createCacheMemories(&memictrl[i], &cim[i], &memdctrl[i], &cdm[i]);
    }


    // zero the control (although only the valid bit should be zeroed, rest is don't care)
    for(int c(0); c < sim.doNbcore(); ++c)
        for(int i(0); i < Sets; ++i)
        {
            memictrl[c][i] = 0;
            memdctrl[c][i] = 0;
        }

    for(int i(0); i < sim.doNbcore(); ++i)
        sim.setCore(0, memdctrl[i], (*reinterpret_cast<unsigned int (*)[Sets][Blocksize][Associativity]>(cdm[i])));

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
/** cache memories **/    ptrtocache(cim[0]), ptrtocache(cdm[0]),
/* control memories */    memictrl[0], memdctrl[0],
                          ptrtocache(cim[1]), ptrtocache(cdm[1]),
                          memictrl[1], memdctrl[1]
                  #ifndef __HLS__
                      , &sim
                  #endif
                  ));
    }

    sim.writeBack();

#ifndef __HLS__
    printf("Successfully executed %lld instructions in %lld cycles\n", sim.getCore(0)->csrs.minstret.to_int64(), sim.getCore(0)->csrs.mcycle.to_int64());
    fprintf(stderr, "Successfully executed %lld instructions in %lld cycles\n", sim.getCore(0)->csrs.minstret.to_int64(), sim.getCore(0)->csrs.mcycle.to_int64());

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

    for(int i(0); i < sim.doNbcore(); ++i)
    {
        deleteCacheMemories(&memictrl[i], &cim[i], &memdctrl[i], &cdm[i]);
    }

    delete[] cim;
    delete[] cdm;
    delete[] memictrl;
    delete[] memdctrl;

    CCS_RETURN(0);
}
