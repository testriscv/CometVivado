#include <memoryInterface.h>

void IncompleteMemory::process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn,
                               ac_int<32, false>& dataOut, bool& waitOut)
{
  // no latency, wait is always set to false
  waitOut = false;
  if (opType == STORE) {
    data[addr >> 2] = dataIn;
  } else if (opType == LOAD) {
    dataOut = data[addr >> 2];
  }
}

void SimpleMemory::process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn,
                           ac_int<32, false>& dataOut, bool& waitOut)
{
  // no latency, wait is always set to false

  ac_int<32, true> temp;
  ac_int<8, false> t8;
  ac_int<1, true> bit;
  ac_int<16, false> t16;

  switch (opType) {
    case STORE:
      switch (mask) {
        case BYTE:
          temp = data[addr >> 2];
          data[addr >> 2].set_slc(((int)addr.slc<2>(0)) << 3, dataIn.slc<8>(0));
          break;
        case HALF:
          temp = data[addr >> 2];
          data[addr >> 2].set_slc(addr[1] ? 16 : 0, dataIn.slc<16>(0));

          break;
        case WORD:
          temp            = data[addr >> 2];
          data[addr >> 2] = dataIn;
          break;
      }
      break;
    case LOAD:
      switch (mask) {
        case BYTE:
          t8  = data[addr >> 2].slc<8>(((int)addr.slc<2>(0)) << 3);
          bit = t8.slc<1>(7);
          dataOut.set_slc(0, t8);
          dataOut.set_slc(8, (ac_int<24, true>)bit);
          break;
        case HALF:
          t16 = data[addr >> 2].slc<16>(addr[1] ? 16 : 0);
          bit = t16.slc<1>(15);
          dataOut.set_slc(0, t16);
          dataOut.set_slc(16, (ac_int<16, true>)bit);
          break;
        case WORD:
          dataOut = data[addr >> 2];
          break;
        case BYTE_U:
          dataOut = data[addr >> 2].slc<8>(((int)addr.slc<2>(0)) << 3) & 0xff;
          break;
        case HALF_U:
          dataOut = data[addr >> 2].slc<16>(addr[1] ? 16 : 0) & 0xffff;
          break;
      }

      break;
  }
  waitOut = false;
}
