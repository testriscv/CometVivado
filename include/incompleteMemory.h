#ifndef __INCOMPLETE_MEMORY_H__
#define __INCOMPLETE_MEMORY_H__

#include "memoryInterface.h"
#include "memory.h"

class IncompleteMemory: public MemoryInterface {
protected:
  ac_int<32, false> data[DRAM_SIZE>>2];

public:
  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn, ac_int<32, false>& dataOut, bool& waitOut) {
    //no latency, wait is always set to false
    waitOut = false;
    switch(opType) {
      case STORE:
      data[addr>>2] = dataIn;
      break;
      case LOAD:
      dataOut = data[addr>>2];
      break;
    }
  }
};

#endif //__INCOMPLETE_MEMORY_H__
