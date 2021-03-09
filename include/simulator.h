#ifndef __SIMULATOR_H__
#define __SIMULATOR_H__

#include "core.h"

class Simulator {
protected:
  Core core;
  bool exitFlag;


public:
  int breakpoint;
  int timeout;
  virtual void run()
  {
    exitFlag = false;
    while (!exitFlag) {
      doCycle(core, 0);
      solveSyscall();
      extend();
      printCycle();
      //We handle breakpoints
      if (this->breakpoint != -1 && core.cycle == this->breakpoint){
       printf("Reached break\n");
      }

      if (this->core.cycle >= this->timeout){
        printf("Timeout!\n");
        break;
      }
    }
    printEnd();
  }

  virtual void printCycle()   = 0;
  virtual void printEnd()     = 0;
  virtual void extend()       = 0;
  virtual void solveSyscall() = 0;
};

#endif // __SIMULATOR_H__
