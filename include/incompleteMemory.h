#ifndef __INCOMPLETE_MEMORY_H__
#define __INCOMPLETE_MEMORY_H__

#include "memoryInterface.h"
#include "memory.h"

class IncompleteMemory: public MemoryInterface {
public:
  ac_int<32, false> *data;

public:
  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn, ac_int<32, false>& dataOut, bool& waitOut) {
    //no latency, wait is always set to false
    waitOut = false;
    if (opType == STORE){
        data[addr>>2] = dataIn;
    }
    else if (opType == LOAD){
        dataOut = data[addr>>2];
        fprintf(stderr, "Read %x\n", dataOut);
    }

  }
};

#endif //__INCOMPLETE_MEMORY_H__
