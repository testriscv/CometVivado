
/*
 * Idea of the file : open and run a test file
 * The test file is organized as a set of line, each line is :
 * name; instr; numberCycles; [initialRegs[32],]; initialPC; (address, data);
 * finalRegs[32]; finalPC; (address, data); |                 initial state |
 * final state              |
 */

#include <fstream>
#include <iostream>
#include <sstream>

#include <core.h>

struct processorState {
  unsigned int regs[32];
  unsigned int pc;
  unsigned int address;
  unsigned int value;
};

std::istream& operator>>(std::istream& is, struct processorState& state)
{

  int n;
  for (int oneReg = 0; oneReg < 32; oneReg++) {
    is >> std::hex >> state.regs[oneReg];
    is.ignore(2);
  }
  is.ignore(1);
  is >> std::hex >> state.pc;
  is.ignore(2);
  is >> std::hex >> state.address;
  is.ignore(2);
  is >> std::hex >> state.value;
  is.ignore(2, '\n');
  return is;
}

bool getTest(std::istream& is, std::string& message, unsigned int& instruction, unsigned int& numberOfCycles,
             struct processorState& initialState, struct processorState& finalState)
{

  char currentChar;
  while ((is.peek() != ';') && (is.readsome(&currentChar, 1)))
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
  // Opening test definition file
  std::ifstream infile(argv[1]);

  while (infile.peek() != EOF) {
    std::cerr << "Testing ";

    unsigned int instruction, numberOfCycles;
    struct processorState initialState, finalState;

    std::string message("");

    getTest(infile, message, instruction, numberOfCycles, initialState, finalState);
    std::cerr << message;
    numberOfCycles = 35;

    // We initialize a simulator with the state
    Core core;
    ac_int<32, false> im[8192], dm[8192];

    core.im = new CacheMemory(new IncompleteMemory(im), false);
    core.dm = new CacheMemory(new IncompleteMemory(dm), true);

    core.pc = initialState.pc;
    for (int oneReg = 0; oneReg < 32; oneReg++)
      core.regFile[oneReg] = initialState.regs[oneReg];

    im[0]                         = instruction;
    dm[initialState.address >> 2] = initialState.value;
    fprintf(stderr, "Setting %x at %d\n", initialState.value, initialState.address);

    core.ftoDC.we = false;

    core.dctoEx.pc          = 0;
    core.dctoEx.instruction = 0;

    core.dctoEx.opCode = 0;
    core.dctoEx.funct7 = 0;
    core.dctoEx.funct3 = 0;

    core.dctoEx.lhs   = 0;
    core.dctoEx.rhs   = 0;
    core.dctoEx.datac = 0;

    // For branch unit
    core.dctoEx.nextPCDC = 0;
    core.dctoEx.isBranch = false;

    // Information for forward/stall unit
    core.dctoEx.useRs1 = false;
    core.dctoEx.useRs2 = false;
    core.dctoEx.useRs3 = false;
    core.dctoEx.useRd  = false;
    core.dctoEx.rs1    = 0;
    core.dctoEx.rs2    = 0;
    core.dctoEx.rs3    = 0;
    core.dctoEx.rd     = 0;

    // Register for all stages
    core.dctoEx.we = false;

    core.extoMem.pc          = 0;
    core.extoMem.instruction = 0;

    core.extoMem.result            = 0;
    core.extoMem.rd                = 0;
    core.extoMem.useRd             = false;
    core.extoMem.isLongInstruction = false;
    core.extoMem.opCode            = 0;
    core.extoMem.funct3            = 0;

    core.extoMem.datac = 0;

    // For branch unit
    core.extoMem.nextPC   = 0;
    core.extoMem.isBranch = false;

    // Register for all stages
    core.extoMem.we = false;

    core.memtoWB.result = 0;
    core.memtoWB.rd     = 0;
    core.memtoWB.useRd  = false;

    core.memtoWB.address      = 0;
    core.memtoWB.valueToWrite = 0;
    core.memtoWB.byteEnable   = 0;
    core.memtoWB.isStore      = false;
    core.memtoWB.isLoad       = false;

    // Register for all stages
    core.memtoWB.we = false;

    std::cout << printDecodedInstrRISCV(instruction) << std::endl;
    for (int oneCycle = 0; oneCycle < numberOfCycles; oneCycle++) {
      fprintf(stderr, "cycle\n");
      doCycle(core, 0);
    }

    bool worked = true;
    for (int oneReg = 0; oneReg < 32; oneReg++) {
      if (core.regFile[oneReg] != finalState.regs[oneReg]) {
        std::cerr << std::endl
                  << "\t Error: value for reg " << oneReg << " is " << core.regFile[oneReg] << " but should be "
                  << finalState.regs[oneReg];
        worked = false;
      }
    }
    if (dm[finalState.address >> 2] != finalState.value) {
      std::cerr << std::endl
                << "\t Error: memory value at address " << finalState.address << " is " << dm[finalState.address]
                << " but should be " << finalState.value << std::endl;
      worked = false;
    }

    if (worked)
      std::cerr << "  OK" << std::endl;
    else
      std::cerr << std::endl;
  }

  return 0;
}
