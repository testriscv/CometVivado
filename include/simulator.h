/** Copyright 2021 INRIA, Université de Rennes 1 and ENS Rennes
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*       http://www.apache.org/licenses/LICENSE-2.0
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*/



#ifndef __SIMULATOR_H__
#define __SIMULATOR_H__

#include "core.h"
#include "ac_int.h"


#define DRAM_SIZE ((size_t)1 << 26)
#define STACK_INIT (DRAM_SIZE - 0x1000)

class Simulator {
protected:
  Core core;
  bool exitFlag;
  bool monitor = false;



public:
  int breakpoint;
  int timeout;
  int nbUnrecognizedDC = 0;
  int nbUnrecognizedExec = 0;
  int nbOutOfInstrMem = 0;
  int nbOutOfMem = 0;

  unsigned int textStart, textEnd;

  virtual void run()
  {
    exitFlag  = false;
    while (!exitFlag) {
      doCycle(core, 0);
      solveSyscall();
      extend();
      printCycle();
      //We handle breakpoints
      if (this->breakpoint != -1 && core.cycle == this->breakpoint){
        if (monitor){
 	        printCoreReg("BeforeInj.txt");
        }
        printf("Reached break\n");
      }

      if (this->core.cycle >= this->timeout){
        printf("Timeout!\n");
        break;
      }

      if (monitor){
        if (!isRecognized(this->core.ftoDC.instruction)){
          nbUnrecognizedDC++;
        }
        if (!isRecognized(this->core.dctoEx.instruction)){
          nbUnrecognizedExec++;
        }
        if (core.pc > textEnd || core.pc < textStart){
          nbOutOfInstrMem++;
        }
        if (core.memtoWB.address > DRAM_SIZE && (core.memtoWB.isLoad || core.memtoWB.isStore)){
          nbOutOfMem++;
        }
        printCoreReg();
      }
    }
    printEnd();

	printf("\nCore cycle: %ld\n", this->core.cycle);
  }


  virtual void printCycle()   = 0;
  virtual void printEnd()     = 0;
  virtual void extend()       = 0;
  virtual void solveSyscall() = 0;
  virtual void printCoreReg(const char* strTemp = "default") = 0;
};

#endif // __SIMULATOR_H__
