#ifndef __SIMULATOR_H__
#define __SIMULATOR_H__

#include "core.h"

class Simulator
{
protected:
    Core core;
    ac_int<32, false>* instructionMemory;
    ac_int<32, false>* dataMemory;
    bool exitFlag;

public:
    virtual void run() 
    {
        exitFlag = false;
        while(!exitFlag){
            doCycle(core, instructionMemory, dataMemory, 0);
            solveSyscall();
            extend();
            printCycle();
        }
        printStat();
    }

    virtual void printCycle() = 0;
    virtual void printStat() = 0;
    virtual void extend() = 0;
    virtual void solveSyscall() = 0;
};

#endif // __SIMULATOR_H__
