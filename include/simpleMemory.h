#ifndef __SIMPLE_MEMORY_H__
#define __SIMPLE_MEMORY_H__

#include "memoryInterface.h"
#include "memory.h"

class SimpleMemory: public MemoryInterface {
protected:
  ac_int<32, false> data[DRAM_SIZE>>2];

public:

  //void setByte(ac_int<32, false> addr, ac_int<8, false> data);
  //ac_int<8, false> getByte(ac_int<32, false> addr);
  //void setWord(ac_int<32, false> addr, ac_int<32, false> data);
  //ac_int<32, false> getWord(ac_int<32, false> addr);

  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn, ac_int<32, false>& dataOut, bool& waitOut);
};

#endif //__SIMPLE_MEMORY_H__
