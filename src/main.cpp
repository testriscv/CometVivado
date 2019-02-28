#include <stdlib.h>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include "basic_simulator.h"


int main(int argc, char** argv)
{
    std::string binaryFile;
    std::string inputFile;
    std::string outputFile;
    std::string traceFile;
    std::vector<std::string> benchArgs;

    namespace po = boost::program_options;
    po::options_description optionsDescription("Options");
    po::positional_options_description positionalOpts;
    po::variables_map optionMap;

    try{
        optionsDescription.add_options()
        ("help,h", "Display help message")
        ("file,f", po::value<std::string>()->required(), "Specifies the RISCV program binary file")
        ("input,i", po::value<std::string>(), "Specifies input file")
        ("output,o", po::value<std::string>(), "Specifies output file")
        ("trace-file,t", po::value<std::string>(), "Specifies trace file for simulator output")
        ("program-args,a", po::value<std::vector<std::string> >(), "Specifies command line arguments for the binary program");

        positionalOpts.add("program-args", -1);

        po::command_line_parser parser{argc, argv};
        parser.options(optionsDescription);
        parser.positional(positionalOpts);
        po::store(parser.run(), optionMap);

        if(optionMap.count("help")){
            std::cout << "Usage: "<< argv[0] << " -f FILE [options] [-a [program-args]]\n" << optionsDescription;
            return 0;
        }

        po::notify(optionMap);

        binaryFile = optionMap["file"].as<std::string>();
        if(optionMap.count("program-args")){
            auto pargs = optionMap["program-args"].as<std::vector<std::string> >();
            benchArgs.push_back(binaryFile);
            for(auto a: pargs){
                benchArgs.push_back(a);
            }
        }

        if(optionMap.count("input"))
          inputFile = optionMap["input"].as<std::string>();
        if(optionMap.count("output"))
          outputFile = optionMap["output"].as<std::string>();
        if(optionMap.count("trace-file"))
          traceFile = optionMap["trace-file"].as<std::string>();

    }catch(std::exception& e){
        std::cerr << "Error: " << e.what() << "\n";
        std::cout << "Usage: "<< argv[0] << " -f FILE [options] [-a [program-args]]\n" << optionsDescription;
        return -1;
    }
    
    BasicSimulator sim(binaryFile.c_str(), benchArgs, inputFile.c_str(), outputFile.c_str(), traceFile.c_str());

    sim.run();

    return 0;
}
