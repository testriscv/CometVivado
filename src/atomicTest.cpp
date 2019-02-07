
/*
 * Idea of the file : open and run a test file
 * The test file is organized as a set of line, each line is :
 * name; instr; numberCycles; [initialRegs[32],]; initialPC; (address, data); finalRegs[32]; finalPC; (address, data);
 *                            |                 initial state              |             final state              |
 */

#include <iostream>
#include <fstream>
#include <sstream>

#include <core.h>

struct processorState{
	unsigned int regs[32];
	unsigned int pc;
	unsigned int address;
	unsigned int value;
};

std::istream& operator>>(std::istream& is, struct processorState& state) {

	int n;
	for (int oneReg=0; oneReg<32; oneReg++){
    	is >> std::hex >> state.regs[oneReg];
    	is.ignore(2);
	}
    is.ignore(1);
	is >> std::hex >> state.pc;
	is.ignore(2);
	is >> std::hex >> state.address;
	is.ignore(2);
	is >> std::hex >>state.value;
	is.ignore(2, '\n');
    return is;
}

bool getTest(std::istream& is, std::string &message, unsigned int &instruction, unsigned int &numberOfCycles, struct processorState &initialState, struct processorState &finalState){

	char currentChar;
	while ((is.peek()!=';') && (is.readsome(&currentChar, 1)))
		message += currentChar;

	is.ignore(2);
	is >> std::hex >> instruction;
	is.ignore(2);
	is >> std::hex >> numberOfCycles;
	is.ignore(3);
	is >> initialState;
	is.ignore(1);
	is >> finalState;
	return true;
}

int main(int argc, char** argv)
{
	//Opening test definition file
	std::ifstream infile("test.txt");


	while (infile.peek() != EOF){
		std::cerr<<"Testing ";

		unsigned int instruction, numberOfCycles;
		struct processorState initialState, finalState;

		std::string message("");

		getTest(infile, message, instruction, numberOfCycles, initialState, finalState);
		std::cerr<<message;

		//We initialize a simulator with the state
		Core core;
		core.pc = initialState.pc;
		for (int oneReg = 0; oneReg<32; oneReg++)
			core.regFile[oneReg] = initialState.regs[oneReg];



		ac_int<32, false> im[8192], dm[8192];

		im[0] = instruction;

		for (int oneCycle = 0; oneCycle < numberOfCycles; oneCycle++){
			doCycle(core, im, dm, 0);
		}


		bool worked = true;
		for (int oneReg = 0; oneReg<32; oneReg++){
			if (core.regFile[oneReg] != finalState.regs[oneReg]){
				std::cerr<< std::endl << "\t Error: value for reg " << oneReg << " is " << core.regFile[oneReg] << " but should be " << finalState.regs[oneReg];
				worked = false;
			}
		}
		if (dm[finalState.address>>2] != finalState.value){
			std::cerr<< std::endl << "\t Error: memory value at address " << finalState.address << " is " << dm[finalState.address]<< " but should be " << finalState.value << std::endl;
			worked = false;
		}

		if (worked)
			std::cerr << "  OK" << std::endl;
		else
			std::cerr << std::endl;

	}

	return 0;


}
