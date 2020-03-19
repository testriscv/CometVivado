#ifndef __MEMORY_INTERFACE_H__
#define __MEMORY_INTERFACE_H__

#include <ac_int.h>

typedef enum { BYTE = 0, HALF, WORD, BYTE_U, HALF_U, LONG } memMask;

typedef enum { NONE = 0, LOAD, STORE } memOpType;

template <unsigned int INTERFACE_SIZE> class MemoryInterface {
protected:
  bool wait;

public:
  virtual void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<INTERFACE_SIZE * 8, false> dataIn,
                       ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut) = 0;
};

template <unsigned int INTERFACE_SIZE> class IncompleteMemory : public MemoryInterface<INTERFACE_SIZE> {
public:
  ac_int<32, false>* data;

public:
  IncompleteMemory(ac_int<32, false>* arg) { data = arg; }
  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<INTERFACE_SIZE * 8, false> dataIn,
               ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut)
  {

    // Incomplete memory only works for 32 bits
    assert(INTERFACE_SIZE == 4);

    // no latency, wait is always set to false
    waitOut = false;
    if (opType == STORE) {
      data[(addr >> 2) & 0xffffff] = dataIn;
    } else if (opType == LOAD) {
      dataOut = data[(addr >> 2) & 0xffffff];
    }
  }
};

template <unsigned int INTERFACE_SIZE> class SimpleMemory : public MemoryInterface<INTERFACE_SIZE> {
public:
  ac_int<32, false>* data;

  SimpleMemory(ac_int<32, false>* arg) { data = arg; }
  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<INTERFACE_SIZE * 8, false> dataIn,
               ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut)
  {
    // no latency, wait is always set to false

    ac_int<32, true> temp;
    ac_int<8, false> t8;
    ac_int<1, true> bit;
    ac_int<16, false> t16;

    switch (opType) {
      case STORE:
        switch (mask) {
          case BYTE_U:
          case BYTE:
            temp = data[addr >> 2];
            data[addr >> 2].set_slc(((int)addr.slc<2>(0)) << 3, dataIn.template slc<8>(0));
            break;
          case HALF:
          case HALF_U:
            temp = data[addr >> 2];
            data[addr >> 2].set_slc(addr[1] ? 16 : 0, dataIn.template slc<16>(0));

            break;
          case WORD:
            temp            = data[addr >> 2];
            data[addr >> 2] = dataIn;
            break;
          case LONG:
            for (int oneWord = 0; oneWord < INTERFACE_SIZE / 4; oneWord++)
              data[(addr >> 2) + oneWord] = dataIn.template slc<32>(32 * oneWord);
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
          case LONG:
            for (int oneWord = 0; oneWord < INTERFACE_SIZE / 4; oneWord++)
              dataOut.set_slc(32 * oneWord, data[(addr >> 2) + oneWord]);
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
};

#endif //__MEMORY_INTERFACE_H__
