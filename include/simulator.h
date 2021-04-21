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

  virtual void printInfo(const char* strTemp = "default")
	{
		std::ofstream pFile;

		if (strTemp == "default")
			if (this->breakpoint == -1)
				pFile.open("goldenPipelineReg.txt");
			else 
				pFile.open("faultyPipelineReg.txt");
		else 
			pFile.open(strTemp);

		if (pFile.is_open())
		{
			pFile << "pipleline registers:";		
			pFile << "\ncore.ftoDC.pc: " <<  (unsigned int)this->core.ftoDC.pc;
			pFile << "\ncore.ftoDC.instruction: " <<  (unsigned int)this->core.ftoDC.instruction;
			pFile << "\ncore.ftoDC.nextPCFetch:  " <<  (unsigned int)this->core.ftoDC.nextPCFetch;
			pFile << "\ncore.ftoDC.we: " <<  (unsigned int)this->core.ftoDC.we;

			pFile << "\ncore.dctoEx.pc: " <<  (unsigned int)this->core.dctoEx.pc;
			pFile << "\ncore.dctoEx.instruction: " <<  (unsigned int)this->core.dctoEx.instruction;
			pFile << "\ncore.dctoEx.opCode:  " <<  (unsigned int)this->core.dctoEx.opCode;
			pFile << "\ncore.dctoEx.funct7: " <<  (unsigned int)this->core.dctoEx.funct7;
			pFile << "\ncore.dctoEx.funct3: " <<  (unsigned int)this->core.dctoEx.funct3;
			pFile << "\ncore.dctoEx.lhs: " <<  (signed int)this->core.dctoEx.lhs;
			pFile << "\ncore.dctoEx.rhs: " <<  (signed int)this->core.dctoEx.rhs;
			pFile << "\ncore.dctoEx.datac: " <<  (signed int)this->core.dctoEx.datac;
			pFile << "\ncore.dctoEx.nextPCDC: " <<  (unsigned int)this->core.dctoEx.nextPCDC;
			pFile << "\ncore.dctoEx.isBranch: " <<  (unsigned int)this->core.dctoEx.isBranch;
			pFile << "\ncore.dctoEx.useRs1: " <<  (unsigned int)this->core.dctoEx.useRs1;
			pFile << "\ncore.dctoEx.useRs2: " <<  (unsigned int)this->core.dctoEx.useRs2;
			pFile << "\ncore.dctoEx.useRs3: " <<  (unsigned int)this->core.dctoEx.useRs3;
			pFile << "\ncore.dctoEx.useRd: " <<  (unsigned int)this->core.dctoEx.useRd;
			pFile << "\ncore.dctoEx.rs1: " <<  (unsigned int)this->core.dctoEx.rs1;
			pFile << "\ncore.dctoEx.rs2: " <<  (unsigned int)this->core.dctoEx.rs2;
			pFile << "\ncore.dctoEx.rs3: " <<  (unsigned int)this->core.dctoEx.rs3;
			pFile << "\ncore.dctoEx.rd: " <<  (unsigned int)this->core.dctoEx.rd;
			pFile << "\ncore.dctoEx.we: " <<  (unsigned int)this->core.dctoEx.we;

			pFile << "\ncore.extoMem.pc: " <<  (unsigned int)this->core.extoMem.pc;
			pFile << "\ncore.extoMem.instruction: " <<  (unsigned int)this->core.extoMem.instruction;
			pFile << "\ncore.extoMem.result: " <<  (signed int)this->core.extoMem.result;
			pFile << "\ncore.extoMem.rd: " <<  (unsigned int)this->core.extoMem.rd;
			pFile << "\ncore.extoMem.useRd: " <<  (unsigned int)this->core.extoMem.useRd;
			pFile << "\ncore.extoMem.isLongInstruction: " <<  (unsigned int)this->core.extoMem.isLongInstruction;
			pFile << "\ncore.extoMem.opCode: " <<  (unsigned int)this->core.extoMem.opCode;
			pFile << "\ncore.extoMem.funct3: " <<  (unsigned int)this->core.extoMem.funct3;
			pFile << "\ncore.extoMem.datac: " <<  (signed int)this->core.extoMem.datac;
			pFile << "\ncore.extoMem.nextPC: " <<  (unsigned int)this->core.extoMem.nextPC;
			pFile << "\ncore.extoMem.isBranch: " <<  (unsigned int)this->core.extoMem.isBranch;
			pFile << "\ncore.extoMem.we: " <<  (unsigned int)this->core.extoMem.we;

			pFile << "\ncore.memtoWB.result: " <<  (signed int)this->core.memtoWB.result;
			pFile << "\ncore.memtoWB.rd: " <<  (unsigned int)this->core.memtoWB.rd;
			pFile << "\ncore.memtoWB.useRd: " <<  (unsigned int)this->core.memtoWB.useRd;
			pFile << "\ncore.memtoWB.address: " <<  (signed int)this->core.memtoWB.address;
			pFile << "\ncore.memtoWB.valueToWrite: " <<  (unsigned int)this->core.memtoWB.valueToWrite;
			pFile << "\ncore.memtoWB.byteEnable: " <<  (unsigned int)this->core.memtoWB.byteEnable;
			pFile << "\ncore.memtoWB.isStore: " <<  (unsigned int)this->core.memtoWB.isStore;
			pFile << "\ncore.memtoWB.isLoad: " <<  (unsigned int)this->core.memtoWB.isLoad;
			pFile << "\ncore.memtoWB.we: " <<  (unsigned int)this->core.memtoWB.we;
			pFile << "\n";
			pFile.close();
		} else 
			printf("\nUnable to open pFile\n");
		//////////////////////////////////////////////
		// pFile << "\ncore.dm: " <<  (unsigned int)this->core.dm;
		// pFile << "\ncore.im: " <<  (unsigned int)this->core.im;
		/////////////////////////////////////////////
		std::ofstream rFile;

		if (strTemp == "default")
			if (this->breakpoint == -1)
				rFile.open("goldenRegFile.txt");
			else
				rFile.open("faultyRegFile.txt");
		else 
			rFile.open(strTemp, std::ios_base::app);

		if (rFile.is_open())
		{
			rFile << "core RegFiles:";		
			int i = 0;
			for (const auto &reg : this->core.regFile)
				rFile << "\nRegFile[" << i++ << "]: " << (signed int)reg;
			rFile << "\n";
			rFile.close();
		} else 
			printf("\nUnable to open rFile\n");
		////////////////////////////////////////////////
		std::ofstream cFile;

		if (strTemp == "default")
			if (this->breakpoint == -1)
				cFile.open("goldenCntrlSignal.txt");
			else
				cFile.open("faultyCntrlSignal.txt");
		else 
			cFile.open(strTemp, std::ios_base::app);
			
		if (cFile.is_open())
		{
			cFile << "core cntrl signals:";
			cFile << "\ncore.pc :   " << (unsigned int)this->core.pc;
			cFile << "\ncore.stallSignals[0] :   " << (unsigned int)this->core.stallSignals[0];
			cFile << "\ncore.stallSignals[1] :   " << (unsigned int)this->core.stallSignals[1];
			cFile << "\ncore.stallSignals[2] :   " << (unsigned int)this->core.stallSignals[2];
			cFile << "\ncore.stallSignals[3] :   " << (unsigned int)this->core.stallSignals[3];
			cFile << "\ncore.stallSignals[4] :   " << (unsigned int)this->core.stallSignals[4];
			cFile << "\ncore.stallIm :   " << (unsigned int)this->core.stallIm;
			cFile << "\ncore.stallDm :   " << (unsigned int)this->core.stallDm;
			cFile << "\ncore.cycle :   " << (unsigned long int)this->core.cycle;
			cFile << "\n";
			cFile.close();
		} else 
			printf("\nUnable to open cFile\n");
	}


  virtual void printCycle()   = 0;
  virtual void printEnd()     = 0;
  virtual void extend()       = 0;
  virtual void solveSyscall() = 0;
};

#endif // __SIMULATOR_H__
