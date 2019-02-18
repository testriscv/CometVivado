#ifndef __SIMPLE_MEMORY_H__
#define __SIMPLE_MEMORY_H__

#include "memoryInterface.h"
#include "memory.h"

class SimpleMemory: public MemoryInterface {
public:
  ac_int<32, false> *data;
public:
  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn, ac_int<32, false>& dataOut, bool& waitOut);
};

#endif //__SIMPLE_MEMORY_H__
