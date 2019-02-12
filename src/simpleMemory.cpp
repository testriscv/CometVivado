#include "simpleMemory.h"
/*
void SimpleMemory::setByte(ac_int<32, false> addr, ac_int<8, false> data) {
  data[addr>>2].set_slc(addr.slc<2>(0) << 3, data);
}

ac_int<8, false> SimpleMemory::getByte(ac_int<32, false> addr) {
  return data[addr>>2].slc<8>(0);
}

void SimpleMemory::setWord(ac_int<32, false> addr, ac_int<32, false> data) {
  data[addr>>2] = data;
}

ac_int<32, false> SimpleMemory::getWord(ac_int<32, false> addr) {
  return  data[addr>>2];
}
*/
void SimpleMemory::process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn, ac_int<32, false>& dataOut, bool& waitOut) {
  //no latency, wait is always set to false
  waitOut = false;
  switch(opType) {
    case STORE:
      switch(mask) {
        case BYTE:
          data[addr>>2].set_slc(addr.slc<2>(0) << 3, dataIn.slc<8>(0));
          break;
        case HALF:
          data[addr>>2].set_slc(addr[1], dataIn.slc<16>(0));
          break;
        case WORD:
          data[addr>>2] = dataIn;
          break;
      }
      break;
    case LOAD:
      switch(mask) {
        case BYTE:
          dataOut = data[addr>>2] & 0xff;
          break;
        case HALF:
          dataOut = data[addr>>2] & 0xffff;
          break;
        case WORD:
          dataOut = data[addr>>2];
          break;
      }
  }
}
