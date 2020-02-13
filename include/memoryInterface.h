#ifndef __MEMORY_INTERFACE_H__
#define __MEMORY_INTERFACE_H__

#include <ac_int.h>

typedef enum { BYTE = 0, HALF, WORD, BYTE_U, HALF_U } memMask;

typedef enum { NONE = 0, LOAD, STORE } memOpType;

class MemoryInterface {
protected:
  bool wait;

public:
  virtual void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn,
                       ac_int<32, false>& dataOut, bool& waitOut) = 0;
};

#endif //__MEMORY_INTERFACE_H__
