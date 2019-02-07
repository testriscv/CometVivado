#include <stdlib.h>
#include <string.h>

#include "basic_simulator.h"

void parseArgs(int argc, char** argv, char *& binFile, char *& inFile, char *& outFile, int &benchArgc, char **&benchArgv);

int main(int argc, char** argv)
{
    char *binaryFile;
    char *inputFile; 
    char *outputFile;
    char **benchargv;
    int benchargc;

    parseArgs(argc, argv, binaryFile, inputFile, outputFile, benchargc, benchargv);

    BasicSimulator sim(binaryFile, benchargc, benchargv, inputFile, outputFile);

    sim.run();

    return 0;
}

void parseArgs(int argc, char** argv, char *& binFile, char *& inFile, char *& outFile, int &benchArgc, char **&benchArgv)
{
    int argstart = 0;
    bool binaryFile = false;
    inFile = NULL;
    outFile = NULL;
    benchArgc = 1;
    for(int i = 1; i < argc; ++i)
    {
        if(strcmp("-i", argv[i]) == 0)
        {
            inFile = argv[i+1];
        }
        else if(strcmp("-o", argv[i]) == 0)
        {
            outFile = argv[i+1];
        }
        else if(strcmp("-f", argv[i]) == 0)
        {
            binaryFile = true;
            binFile = argv[i+1];
        }
        else if(strcmp("--", argv[i]) == 0)
        {
            argstart = i+1;
            benchArgc = argc - i;
            benchArgv = new char*[benchArgc];
            break;
        }
    }

    if(benchArgv == 0)
        benchArgv = new char*[benchArgc];

    if(!binaryFile)
        // TODO: solve warning for conversion from string constant to char* 
#ifdef __HLS__
        binFile = "matmul.riscv32";
#else
        binFile = "benchmarks/build/matmul_int_4.riscv32";
#endif

    benchArgv[0] = new char[strlen(binFile)+1];
    strcpy(benchArgv[0], binFile);
    for(int i = 1; i < benchArgc; ++i)
    {
        benchArgv[i] = new char[strlen(argv[argstart + i - 1])+1];
        strcpy(benchArgv[i], argv[argstart + i - 1]);
        std::cout << "Read benchmark argument " << i << ": " << benchArgv[i] << std::endl;
    }
}
