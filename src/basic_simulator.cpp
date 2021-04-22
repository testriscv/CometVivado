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



#include <errno.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <fstream>

#include <sys/time.h>
#include <unistd.h>

#include "riscvISA.h"
#include "basic_simulator.h"
#include "core.h"
#include "elfFile.h"

#define DEBUG 0

BasicSimulator::BasicSimulator(std::string binaryFile, std::vector<std::string> args,
                               std::string inFile, std::string outFile,
                               std::string tFile, std::string sFile)
{

  memset((char*)&core, 0, sizeof(Core));

  mem.reserve(DRAM_SIZE >> 2);

  core.im = new SimpleMemory<4>(mem.data());
  core.dm = new SimpleMemory<4>(mem.data());

  // core.im = new CacheMemory<4, 16, 64>(new IncompleteMemory<4>(mem.data()), false);
  // core.dm = new CacheMemory<4, 16, 64>(new IncompleteMemory<4>(mem.data()), false);

  openFiles(inFile, outFile, tFile, sFile);

  readElf(binaryFile.c_str());

  pushArgsOnStack(args);

  core.regFile[2] = STACK_INIT;
}

FILE* fopenCheck(const char* fname, const char* mode){
    FILE* fp = fopen(fname, mode);
    if(fp == NULL){
        fprintf(stderr, "Error: cannot open file %s\n", fname);
        exit(-1);
    }
    return fp;
}

void openOrDefault(std::string fname, const char* mode, FILE* def, FILE* &dest){
  if (fname.empty())
    dest = def;
  else
    dest = fopenCheck(fname.c_str(), mode);
}

void BasicSimulator::openFiles(std::string inFile, std::string outFile, std::string tFile, std::string sFile){
  openOrDefault(inFile, "rb", stdin, inputFile);
  openOrDefault(outFile, "wb", stdout, outputFile);
  openOrDefault(tFile, "wb", stderr, traceFile);
  openOrDefault(sFile, "wb", NULL, signatureFile);
}

void BasicSimulator::readElf(const char *binaryFile){
  heapAddress = 0;
  ElfFile elfFile(binaryFile);
  for(auto const &section : elfFile.sectionTable){
    if(section.address != 0){
      for (unsigned i = 0; i < section.size; i++)
        setByte(section.address + i, elfFile.content[section.offset + i]);

       // update the size of the heap
       if (section.name != ".text") {
         if (section.address + section.size > heapAddress)
           heapAddress = section.address + section.size;
       }
    }
  }

  core.pc = find_by_name(elfFile.symbols, "_start").offset;
  if (signatureFile != NULL){
    begin_signature = find_by_name(elfFile.symbols, "begin_signature").offset;
    end_signature = find_by_name(elfFile.symbols, "end_signature").offset;
  }
  if(DEBUG){
    printf("Elf Reading done.\n");
  }
}

void BasicSimulator::pushArgsOnStack(const std::vector<std::string> args){
  unsigned int argc = args.size();

  mem[STACK_INIT >> 2] = argc;

  auto currentPlaceStrings = STACK_INIT + 4 + 4 * argc;
  for (unsigned oneArg = 0; oneArg < argc; oneArg++) {
    mem[(STACK_INIT + 4 * oneArg + 4) >> 2] = currentPlaceStrings;

    int oneCharIndex = 0;
    for(const auto c : args[oneArg]){
      setByte(currentPlaceStrings + oneCharIndex, c);
      oneCharIndex++;
    }
    setByte(currentPlaceStrings + oneCharIndex, 0);
    currentPlaceStrings += oneCharIndex + 1;
  }
  if(DEBUG){
    printf("Populate Data Memory done.\n");
  }
}

BasicSimulator::~BasicSimulator()
{
  if (inputFile)
    fclose(inputFile);
  if (outputFile)
    fclose(outputFile);
  if (traceFile)
    fclose(traceFile);
  if(signatureFile)
    fclose(signatureFile);
}

void BasicSimulator::printCycle()
{
  //print something every cycle
  if(DEBUG){
    if (!core.stallSignals[0] && !core.stallIm && !core.stallDm) {
      printf("Debug trace : %x ", (unsigned int)core.ftoDC.pc);
      std::cout << printDecodedInstrRISCV(core.ftoDC.instruction);

      for (const auto &reg : core.regFile)
        printf("%x  ", (unsigned int)reg);
      std::cout << std::endl;
    }
  }
}

void BasicSimulator::printEnd()
{
  /*
  RISCV-COMPLIANCE Ending Routine to get the signature
  stored between begin_signature and end_signature.
  */
  if(signatureFile != NULL){
    const auto addr_offset = 4;
    const auto begin_offset = ((begin_signature)%4); // correct address alignement

    if(DEBUG){
      printf("BEGIN/END_SIGNATURE: %x/%x (%x)", begin_signature, end_signature, begin_offset);
    }

    //memory read
    for (auto wordNumber = begin_signature - begin_offset; wordNumber < end_signature - begin_offset; wordNumber+=addr_offset)
    {
      fprintf(signatureFile, "%08x\n", (unsigned int)this->ldw(wordNumber));
    }
  }
}

void BasicSimulator::setByte(unsigned addr, ac_int<8, true> value){
  mem[addr >> 2].set_slc((addr % 4) * 8, value);
}

// Function for handling memory accesses
void BasicSimulator::stb(ac_int<32, false> addr, ac_int<8, true> value)
{
  ac_int<32, false> wordRes = 0;
  bool stall                = true;
  bool releaseIDM           = false;
  while (stall)
    core.dm->process(addr, BYTE, STORE, value, wordRes, stall);
}

void BasicSimulator::sth(ac_int<32, false> addr, ac_int<16, true> value)
{
  this->stb(addr + 1, value.slc<8>(8));
  this->stb(addr + 0, value.slc<8>(0));
}

void BasicSimulator::stw(ac_int<32, false> addr, ac_int<32, true> value)
{
  this->stb(addr + 3, value.slc<8>(24));
  this->stb(addr + 2, value.slc<8>(16));
  this->stb(addr + 1, value.slc<8>(8));
  this->stb(addr + 0, value.slc<8>(0));
}

void BasicSimulator::std(ac_int<32, false> addr, ac_int<64, true> value)
{
  this->stb(addr + 7, value.slc<8>(56));
  this->stb(addr + 6, value.slc<8>(48));
  this->stb(addr + 5, value.slc<8>(40));
  this->stb(addr + 4, value.slc<8>(32));
  this->stb(addr + 3, value.slc<8>(24));
  this->stb(addr + 2, value.slc<8>(16));
  this->stb(addr + 1, value.slc<8>(8));
  this->stb(addr + 0, value.slc<8>(0));
}

ac_int<8, true> BasicSimulator::ldb(ac_int<32, false> addr)
{
  ac_int<8, true> result;
  result                    = mem[addr >> 2].slc<8>(((int)addr.slc<2>(0)) << 3);
  ac_int<32, false> wordRes = 0;
  bool stall                = true;
  bool releaseIDM           = false;
  while (stall)
    core.dm->process(addr, BYTE_U, LOAD, 0, wordRes, stall);

  result = wordRes.slc<8>(0);
  return result;
}

// Little endian version
ac_int<16, true> BasicSimulator::ldh(ac_int<32, false> addr)
{
  ac_int<16, true> result = 0;
  result.set_slc(8, this->ldb(addr + 1));
  result.set_slc(0, this->ldb(addr));
  return result;
}

ac_int<32, true> BasicSimulator::ldw(ac_int<32, false> addr)
{
  ac_int<32, true> result = 0;
  result.set_slc(24, this->ldb(addr + 3));
  result.set_slc(16, this->ldb(addr + 2));
  result.set_slc(8, this->ldb(addr + 1));
  result.set_slc(0, this->ldb(addr));
  return result;
}

ac_int<32, true> BasicSimulator::ldd(ac_int<32, false> addr)
{
  ac_int<32, true> result = 0;
  result.set_slc(56, this->ldb(addr + 7));
  result.set_slc(48, this->ldb(addr + 6));
  result.set_slc(40, this->ldb(addr + 5));
  result.set_slc(32, this->ldb(addr + 4));
  result.set_slc(24, this->ldb(addr + 3));
  result.set_slc(16, this->ldb(addr + 2));
  result.set_slc(8, this->ldb(addr + 1));
  result.set_slc(0, this->ldb(addr));

  return result;
}

/********************************************************************************************************************
**  Software emulation of system calls.
**
** Currently all system calls are solved in the simulator. The function solveSyscall check the opcode in the
** extoMem pipeline registers and verifies whether it is a syscall or not. If it is, they solve the forwarding,
** and switch to the correct function according to reg[17].
*********************************************************************************************************************/
void BasicSimulator::solveSyscall()
{

  if ((core.extoMem.opCode == RISCV_SYSTEM) && core.extoMem.instruction.slc<12>(20) == 0 && !core.stallSignals[2] &&
      !core.stallIm && !core.stallDm) {

    ac_int<32, true> syscallId = core.regFile[17];
    ac_int<32, true> arg1      = core.regFile[10];
    ac_int<32, true> arg2      = core.regFile[11];
    ac_int<32, true> arg3      = core.regFile[12];
    ac_int<32, true> arg4      = core.regFile[13];

    if (core.memtoWB.useRd && core.memtoWB.we && !core.stallSignals[3]) {
      if (core.memtoWB.rd == 10)
        arg1 = core.memtoWB.result;
      else if (core.memtoWB.rd == 11)
        arg2 = core.memtoWB.result;
      else if (core.memtoWB.rd == 12)
        arg3 = core.memtoWB.result;
      else if (core.memtoWB.rd == 13)
        arg4 = core.memtoWB.result;
      else if (core.memtoWB.rd == 17)
        syscallId = core.memtoWB.result;
    }

    ac_int<32, true> result = 0;

    switch (syscallId) {
      case SYS_exit:
        exitFlag = 1; // Currently we break on ECALL
        break;
      case SYS_read:
        result = this->doRead(arg1, arg2, arg3);
        break;
      case SYS_write:
        result = this->doWrite(arg1, arg2, arg3);
        break;
      case SYS_brk:
        result = this->doSbrk(arg1);
        break;
      case SYS_open:
        result = this->doOpen(arg1, arg2, arg3);
        break;
      case SYS_openat:
        result = this->doOpenat(arg1, arg2, arg3, arg4);
        break;
      case SYS_lseek:
        result = this->doLseek(arg1, arg2, arg3);
        break;
      case SYS_close:
        result = this->doClose(arg1);
        break;
      case SYS_fstat:
        result = this->doFstat(arg1, arg2);
        break;
      case SYS_stat:
        result = this->doStat(arg1, arg2);
        break;
      case SYS_gettimeofday:
        result = this->doGettimeofday(arg1);
        break;
      case SYS_unlink:
        result = this->doUnlink(arg1);
        break;
      case SYS_exit_group:
        fprintf(stderr, "Syscall : SYS_exit_group\n");
        exitFlag = 1;
        break;
      case SYS_getpid:
        fprintf(stderr, "Syscall : SYS_getpid\n");
        exitFlag = 1;
        break;
      case SYS_kill:
        fprintf(stderr, "Syscall : SYS_kill\n");
        exitFlag = 1;
        break;
      case SYS_link:
        fprintf(stderr, "Syscall : SYS_link\n");
        exitFlag = 1;
        break;
      case SYS_mkdir:
        fprintf(stderr, "Syscall : SYS_mkdir\n");
        exitFlag = 1;
        break;
      case SYS_chdir:
        fprintf(stderr, "Syscall : SYS_chdir\n");
        exitFlag = 1;
        break;
      case SYS_getcwd:
        fprintf(stderr, "Syscall : SYS_getcwd\n");
        exitFlag = 1;
        break;
      case SYS_lstat:
        fprintf(stderr, "Syscall : SYS_lstat\n");
        exitFlag = 1;
        break;
      case SYS_fstatat:
        fprintf(stderr, "Syscall : SYS_fstatat\n");
        exitFlag = 1;
        break;
      case SYS_access:
        fprintf(stderr, "Syscall : SYS_access\n");
        exitFlag = 1;
        break;
      case SYS_faccessat:
        fprintf(stderr, "Syscall : SYS_faccessat\n");
        exitFlag = 1;
        break;
      case SYS_pread:
        fprintf(stderr, "Syscall : SYS_pread\n");
        exitFlag = 1;
        break;
      case SYS_pwrite:
        fprintf(stderr, "Syscall : SYS_pwrite\n");
        exitFlag = 1;
        break;
      case SYS_uname:
        fprintf(stderr, "Syscall : SYS_uname\n");
        exitFlag = 1;
        break;
      case SYS_getuid:
        fprintf(stderr, "Syscall : SYS_getuid\n");
        exitFlag = 1;
        break;
      case SYS_geteuid:
        fprintf(stderr, "Syscall : SYS_geteuid\n");
        exitFlag = 1;
        break;
      case SYS_getgid:
        fprintf(stderr, "Syscall : SYS_getgid\n");
        exitFlag = 1;
        break;
      case SYS_getegid:
        fprintf(stderr, "Syscall : SYS_getegid\n");
        exitFlag = 1;
        break;
      case SYS_mmap:
        fprintf(stderr, "Syscall : SYS_mmap\n");
        exitFlag = 1;
        break;
      case SYS_munmap:
        fprintf(stderr, "Syscall : SYS_munmap\n");
        exitFlag = 1;
        break;
      case SYS_mremap:
        fprintf(stderr, "Syscall : SYS_mremap\n");
        exitFlag = 1;
        break;
      case SYS_time:
        fprintf(stderr, "Syscall : SYS_time\n");
        exitFlag = 1;
        break;
      case SYS_getmainvars:
        fprintf(stderr, "Syscall : SYS_getmainvars\n");
        exitFlag = 1;
        break;
      case SYS_rt_sigaction:
        fprintf(stderr, "Syscall : SYS_rt_sigaction\n");
        exitFlag = 1;
        break;
      case SYS_writev:
        fprintf(stderr, "Syscall : SYS_writev\n");
        exitFlag = 1;
        break;
      case SYS_times:
        fprintf(stderr, "Syscall : SYS_times\n");
        exitFlag = 1;
        break;
      case SYS_fcntl:
        fprintf(stderr, "Syscall : SYS_fcntl\n");
        exitFlag = 1;
        break;
      case SYS_getdents:
        fprintf(stderr, "Syscall : SYS_getdents\n");
        exitFlag = 1;
        break;
      case SYS_dup:
        fprintf(stderr, "Syscall : SYS_dup\n");
        exitFlag = 1;
        break;

        // Custom syscalls
      case SYS_threadstart:
        result = 0;
        break;
      case SYS_nbcore:
        result = 1;
        break;

      default:
        fprintf(stderr, "Syscall : Unknown system call, %d (%x) with arguments :\n", syscallId.to_int(),
                syscallId.to_int());
        fprintf(stderr, "%d (%x)\n%d (%x)\n%d (%x)\n%d (%x)\n", arg1.to_int(), arg1.to_int(), arg2.to_int(),
                arg2.to_int(), arg3.to_int(), arg3.to_int(), arg4.to_int(), arg4.to_int());
        exitFlag = 1;
        break;
    }

    // We write the result and forward
    core.memtoWB.result = result;
    core.memtoWB.rd     = 10;
    core.memtoWB.useRd  = 1;

    if (core.dctoEx.useRs1 && (core.dctoEx.rs1 == 10))
      core.dctoEx.lhs = result;
    if (core.dctoEx.useRs2 && (core.dctoEx.rs2 == 10))
      core.dctoEx.rhs = result;
    if (core.dctoEx.useRs3 && (core.dctoEx.rs3 == 10))
      core.dctoEx.datac = result;
  }
}

ac_int<32, true> BasicSimulator::doRead(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
  char* localBuffer = new char[size.to_int()];
  ac_int<32, true> result;

  if (file == 0 && inputFile)
    result = read(inputFile->_fileno, localBuffer, size);
  else
    result = read(file, localBuffer, size);

  for (int i(0); i < result; i++) {
    this->stb(bufferAddr + i, localBuffer[i]);
  }

  delete[] localBuffer;
  return result;
}

ac_int<32, true> BasicSimulator::doWrite(ac_int<32, false> file, ac_int<32, false> bufferAddr, ac_int<32, false> size)
{
  char* localBuffer = new char[size.to_int()];

  for (int i = 0; i < size; i++)
    localBuffer[i] = this->ldb(bufferAddr + i);

  ac_int<32, true> result = 0;
  if (file == 1 && outputFile) // 3 is the first available file descriptor
  {
    fflush(stdout);
    result = write(outputFile->_fileno, localBuffer, size);
  } else {
    if (file == 1)
      fflush(stdout);
    else if (file == 2)
      fflush(stderr);
    result = write(file, localBuffer, size);
  }
  delete[] localBuffer;
  return result;
}

ac_int<32, true> BasicSimulator::doFstat(ac_int<32, false> file, ac_int<32, false> stataddr)
{
  ac_int<32, true> result = 0;
  struct stat filestat    = {0};

  if (file != 1)
    result = fstat(file, &filestat);

  std(stataddr, filestat.st_dev);               // unsigned long long
  std(stataddr + 8, filestat.st_ino);           // unsigned long long
  stw(stataddr + 16, filestat.st_mode);         // unsigned int
  stw(stataddr + 20, filestat.st_nlink);        // unsigned int
  stw(stataddr + 24, filestat.st_uid);          // unsigned int
  stw(stataddr + 28, filestat.st_gid);          // unsigned int
  std(stataddr + 32, filestat.st_rdev);         // unsigned long long
  std(stataddr + 40, filestat.__pad0);          // unsigned long long
  std(stataddr + 48, filestat.st_size);         // long long
  stw(stataddr + 56, filestat.st_blksize);      // int
  stw(stataddr + 60, filestat.__pad0);          // int
  std(stataddr + 64, filestat.st_blocks);       // long long
  stw(stataddr + 72, filestat.st_atim.tv_sec);  // long
  stw(stataddr + 76, filestat.st_atim.tv_nsec); // long
  stw(stataddr + 80, filestat.st_mtim.tv_sec);  // long
  stw(stataddr + 84, filestat.st_mtim.tv_nsec); // long
  stw(stataddr + 88, filestat.st_ctim.tv_sec);  // long
  stw(stataddr + 92, filestat.st_ctim.tv_nsec); // long
  stw(stataddr + 96, filestat.__pad0);          // long
  stw(stataddr + 100, filestat.__pad0);         // long

  return result;
}

ac_int<32, true> BasicSimulator::doOpen(ac_int<32, false> path, ac_int<32, false> flags, ac_int<32, false> mode)
{
  int oneStringElement = this->ldb(path);
  int index            = 0;
  while (oneStringElement != 0) {
    index++;
    oneStringElement = this->ldb(path + index);
  }

  int pathSize = index + 1;

  char* localPath = new char[pathSize + 1];
  for (int i = 0; i < pathSize; i++)
    localPath[i] = this->ldb(path + i);
  localPath[pathSize] = '\0';

  // convert riscv flags to unix flags
  int riscvflags = flags;
  std::string str;
  if (riscvflags & SYS_O_WRONLY)
    str += "WRONLY, ";
  else if (riscvflags & SYS_O_RDWR)
    str += "RDWR, ";
  else
    str += "RDONLY, ";
  int unixflags = riscvflags & 3; // O_RDONLY, O_WRITE, O_RDWR are the same
  riscvflags ^= unixflags;
  if (riscvflags & SYS_O_APPEND) {
    unixflags |= O_APPEND;
    riscvflags ^= SYS_O_APPEND;
    str += "APPEND, ";
  }
  if (riscvflags & SYS_O_CREAT) {
    unixflags |= O_CREAT;
    riscvflags ^= SYS_O_CREAT;
    str += "CREAT, ";
  }
  if (riscvflags & SYS_O_TRUNC) {
    unixflags |= O_TRUNC;
    riscvflags ^= SYS_O_TRUNC;
    str += "TRUNC, ";
  }
  if (riscvflags & SYS_O_EXCL) {
    unixflags |= O_EXCL;
    riscvflags ^= SYS_O_EXCL;
    str += "EXCL, ";
  }
  if (riscvflags & SYS_O_SYNC) {
    unixflags |= O_SYNC;
    riscvflags ^= SYS_O_SYNC;
    str += "SYNC, ";
  }
  if (riscvflags & SYS_O_NONBLOCK) {
    unixflags |= O_NONBLOCK;
    riscvflags ^= SYS_O_NONBLOCK;
    str += "NONBLOCK, ";
  }
  if (riscvflags & SYS_O_NOCTTY) {
    unixflags |= O_NOCTTY;
    riscvflags ^= SYS_O_NOCTTY;
    str += "NOCTTY";
  }
  int result = open(localPath, unixflags, mode.to_int());

  delete[] localPath;
  return result;
}

ac_int<32, true> BasicSimulator::doOpenat(ac_int<32, false> dir, ac_int<32, false> path, ac_int<32, false> flags,
                                          ac_int<32, false> mode)
{
  fprintf(stderr, "Syscall : SYS_openat not implemented yet...\n");
  exit(-1);
}

ac_int<32, true> BasicSimulator::doClose(ac_int<32, false> file)
{
  if (file > 2) // don't close simulator's stdin, stdout & stderr
  {
    return close(file);
  }

  return 0;
}

ac_int<32, true> BasicSimulator::doLseek(ac_int<32, false> file, ac_int<32, false> ptr, ac_int<32, false> dir)
{
  int result = lseek(file, ptr, dir);
  return result;
}

ac_int<32, true> BasicSimulator::doStat(ac_int<32, false> filename, ac_int<32, false> stataddr)
{
  int oneStringElement = this->ldb(filename);
  int index            = 0;
  while (oneStringElement != 0) {
    index++;
    oneStringElement = this->ldb(filename + index);
  }

  int pathSize = index + 1;

  char* localPath = new char[pathSize + 1];
  for (int i = 0; i < pathSize; i++)
    localPath[i] = this->ldb(filename + i);
  localPath[pathSize] = '\0';

  struct stat filestat;
  int result = stat(localPath, &filestat);

  std(stataddr, filestat.st_dev);               // unsigned long long
  std(stataddr + 8, filestat.st_ino);           // unsigned long long
  stw(stataddr + 16, filestat.st_mode);         // unsigned int
  stw(stataddr + 20, filestat.st_nlink);        // unsigned int
  stw(stataddr + 24, filestat.st_uid);          // unsigned int
  stw(stataddr + 28, filestat.st_gid);          // unsigned int
  std(stataddr + 32, filestat.st_rdev);         // unsigned long long
  std(stataddr + 40, filestat.__pad0);          // unsigned long long
  std(stataddr + 48, filestat.st_size);         // long long
  stw(stataddr + 56, filestat.st_blksize);      // int
  stw(stataddr + 60, filestat.__pad0);          // int
  std(stataddr + 64, filestat.st_blocks);       // long long
  stw(stataddr + 72, filestat.st_atim.tv_sec);  // long
  stw(stataddr + 76, filestat.st_atim.tv_nsec); // long
  stw(stataddr + 80, filestat.st_mtim.tv_sec);  // long
  stw(stataddr + 84, filestat.st_mtim.tv_nsec); // long
  stw(stataddr + 88, filestat.st_ctim.tv_sec);  // long
  stw(stataddr + 92, filestat.st_ctim.tv_nsec); // long
  stw(stataddr + 96, filestat.__pad0);          // long
  stw(stataddr + 100, filestat.__pad0);         // long

  delete[] localPath;
  return result;
}

ac_int<32, true> BasicSimulator::doSbrk(ac_int<32, false> value)
{
  ac_int<32, true> result;
  if (value == 0) {
    result = heapAddress;
  } else {
    heapAddress = value;
    result      = value;
  }

  return result;
}

ac_int<32, true> BasicSimulator::doGettimeofday(ac_int<32, false> timeValPtr)
{
  struct timeval oneTimeVal;
  int result = gettimeofday(&oneTimeVal, NULL);

  this->stw(timeValPtr, oneTimeVal.tv_sec);
  this->stw(timeValPtr + 4, oneTimeVal.tv_usec);

  return result;
}

ac_int<32, true> BasicSimulator::doUnlink(ac_int<32, false> path)
{
  int oneStringElement = this->ldb(path);
  int index            = 0;
  while (oneStringElement != '\0') {
    index++;
    oneStringElement = this->ldb(path + index);
  }

  int pathSize = index + 1;

  char* localPath = new char[pathSize + 1];
  for (int i = 0; i < pathSize; i++)
    localPath[i] = this->ldb(path + i);
  localPath[pathSize] = '\0';

  int result = unlink(localPath);

  delete[] localPath;
  return result;
}


void BasicSimulator::printCoreReg(const char* strTemp = "default")
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
