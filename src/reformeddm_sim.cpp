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

using namespace std;

CCS_MAIN(int argc, char** argv)
{
    printf("Parameters : Size   Blocksize   Associativity   Sets   Policy\n");
    printf("Parameters : %5d   %8d   %13d   %4d   %6d\n", Size, 4*Blocksize, Associativity, Sets, Policy);

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
#if CCS_DUT_SYSC || CCS_DUT_RTL
        binaryFile = "../benchmarks/build/matmul_int_4.riscv";
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

    ac_int<64, false> cycles = 0, numins = 0;
    bool exit = false;
    while(!exit)
    {
        CCS_DESIGN(doStep(sim.getPC(), im, dm, exit
                  #ifndef CCS_DUT_RTL
                      , cycles, numins, &sim
                  #endif
                  ));
        if(cycles > (uint64_t)5e8)
            break;
    }
    printf("Successfully executed %lld instructions in %lld cycles\n", numins.to_int64(), cycles.to_int64());
    fprintf(stderr, "Successfully executed %lld instructions in %lld cycles\n", numins.to_int64(), cycles.to_int64());

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

    for(int i = 0; i < benchargc; ++i)
        delete[] benchargv[i];
    delete[] benchargv;
    delete[] dm;
    delete[] im;
    CCS_RETURN(0);
}
