#include <string>
#include <vector>

#include "CLI11.hpp"
#include "basic_simulator.h"

int main(int argc, char** argv)
{
  std::string binaryFile; // assign to default
  std::string inputFile;
  std::string outputFile;
  std::string traceFile;
  std::string signatureFile;
  std::vector<std::string> benchArgs, pargs;
  std::string breakpoint = "-1";
  std::string timeout = "-1";

  CLI::App app{"Comet RISC-V Simulator"};
  app.add_option("-f,--file", binaryFile, "Specifies the RISC-V program binary file (elf)")->required();
  app.add_option("-i,--input", inputFile,
                 "Specifies the input file (used as standard input of the "
                 "running program)");
  app.add_option("-o,--output", outputFile, "Specifies the output file (standard output of the running program)");
  app.add_option("-t,--trace-file", traceFile, "Specifies trace file for simulator output");
  app.add_option("-a,--program-args", pargs, "Specifies command line arguments for the binary program");
  app.add_option("-s,--signature-output", signatureFile, "Specifies signature file for testing purposes");
  app.add_option("-b,--break", breakpoint, "Provide a breakpoint at the cycle given (along with gdb : break basic_simulator.cpp:129)");
  app.add_option("-e,--end", timeout, "Add a timeout option to the execution (the simulator stops if this number of cycle is reached)");

  CLI11_PARSE(app, argc, argv);

  // add the binary file name at the start of argv[]
  benchArgs.push_back(binaryFile);
  for (auto a : pargs)
    benchArgs.push_back(a);
  BasicSimulator sim(binaryFile, benchArgs, inputFile, outputFile, traceFile, signatureFile);

  sim.breakpoint = std::stoi(breakpoint, NULL);
  sim.timeout = std::stoi(timeout, NULL);

  sim.run();

  return 0;
}
