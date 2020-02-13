#include <stdlib.h>
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
  std::vector<std::string> benchArgs, pargs;

  CLI::App app{"Comet RISC-V Simulator"};
  app.add_option("-f,--file", binaryFile, "Specifies the RISC-V program binary file (elf)")->required();
  app.add_option("-i,--input", inputFile,
                 "Specifies the input file (used as standard input of the "
                 "runing program)");
  app.add_option("-o,--output", outputFile, "Specifies the output file (standard output of the running program)");
  app.add_option("-t,--trace-file", traceFile, "Specifies trace file for simulator output");
  app.add_option("-a,--program-args", pargs, "Specifies command line arguments for the binary program");

  CLI11_PARSE(app, argc, argv);

  // add the binary file name at the start of argv[]
  benchArgs.push_back(binaryFile);
  for (auto a : pargs)
    benchArgs.push_back(a);
  BasicSimulator sim(binaryFile.c_str(), benchArgs, inputFile.c_str(), outputFile.c_str(), traceFile.c_str());

  sim.run();

  return 0;
}
