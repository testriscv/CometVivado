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



#ifndef __BASIC_SIMULATOR_H__
#define __BASIC_SIMULATOR_H__

#include <vector>
#include "ac_int.h"
#include "simulator.h"


class BasicSimulator : public Simulator {
  unsigned heapAddress;

  // Signature address when doing compliance tests
  unsigned int begin_signature, end_signature;

  std::vector<ac_int<32, false> > mem;

  FILE* inputFile;
  FILE* outputFile;
  FILE* traceFile;
  FILE* signatureFile;

public:
  BasicSimulator(const std::string binaryFile, const std::vector<std::string>,
                 const std::string inFile, const std::string outFile,
                 const std::string tFile, const std::string sFile, bool monitor);
  ~BasicSimulator();

protected:
  void printCycle();
  void printEnd();
  void extend(){};
  void printCoreReg(const char* strTemp);

  // Functions for memory accesses
  void stb(const ac_int<32, false> addr, const ac_int<8, true> value);
  void sth(const ac_int<32, false> addr, const ac_int<16, true> value);
  void stw(const ac_int<32, false> addr, const ac_int<32, true> value);
  void std(const ac_int<32, false> addr, const ac_int<64, true> value);

  ac_int<8, true> ldb(const ac_int<32, false> addr);
  ac_int<16, true> ldh(const ac_int<32, false> addr);
  ac_int<32, true> ldw(const ac_int<32, false> addr);
  ac_int<32, true> ldd(const ac_int<32, false> addr);

  // Functions for solving syscalls
  void solveSyscall();

  ac_int<32, true> doRead(const unsigned file, const unsigned bufferAddr, const unsigned size);
  ac_int<32, true> doWrite(const unsigned file, const unsigned bufferAddr, const unsigned size);
  ac_int<32, true> doOpen(const unsigned name, const unsigned flags, const unsigned mode);
  ac_int<32, true> doOpenat(const unsigned dir, const unsigned name, const unsigned flags, const unsigned mode);
  ac_int<32, true> doLseek(const unsigned file, const unsigned ptr, const unsigned dir);
  ac_int<32, true> doClose(const unsigned file);
  ac_int<32, true> doStat(ac_int<32, false> filename, ac_int<32, false> ptr);
  ac_int<32, true> doSbrk(const ac_int<32, false> value);
  ac_int<32, true> doGettimeofday(const ac_int<32, false> timeValPtr);
  ac_int<32, true> doUnlink(const unsigned path);
  ac_int<32, true> doFstat(const unsigned file, const ac_int<32, false> stataddr);

private:
  std::string string_from_mem(const unsigned); // TODO make const
  void setByte(const unsigned, const ac_int<8, true>);
  void readElf(const char*);
  void pushArgsOnStack(const std::vector<std::string>);
  void openFiles(const std::string inFile, const std::string outFile, const std::string tFile, const std::string sFile);
};

#endif // __BASIC_SIMULATOR_H__
