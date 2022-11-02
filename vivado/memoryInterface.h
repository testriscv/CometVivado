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



#ifndef __MEMORY_INTERFACE_H__
#define __MEMORY_INTERFACE_H__

#include "ac_int.h"

typedef enum { BYTE = 0, HALF, WORD, BYTE_U, HALF_U /*, LONG*/ } memMask;

typedef enum { NONE = 0, LOAD, STORE } memOpType;

template <unsigned int INTERFACE_SIZE> class MemoryInterface {
protected:
  bool wait;

public:
  virtual void process(const ac_int<32, false> addr, const memMask mask, const memOpType opType, const ac_int<INTERFACE_SIZE * 8, false> dataIn,
                       ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut) = 0;
};

template <unsigned int INTERFACE_SIZE> class IncompleteMemory : public MemoryInterface<INTERFACE_SIZE> {
public:
  ac_int<32, false>* data;
  ac_int<1, false> pendingWrite;
  ac_int<32, false> valueLoaded;

public:
  IncompleteMemory(ac_int<32, false>* arg) { data = arg; }
  void process(const ac_int<32, false> addr, const memMask mask, const memOpType opType, const ac_int<INTERFACE_SIZE * 8, false> dataIn,
               ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut)
  {
    // Incomplete memory only works for 32 bits
    assert(INTERFACE_SIZE == 4);

    ac_int<32, true> temp;
    ac_int<8, false> t8;
    ac_int<1, true> bit;
    ac_int<16, false> t16;
    ac_int<32, false> mergedAccess;

    if ((!pendingWrite && opType == STORE && mask != WORD) || opType == LOAD) {

      mergedAccess = data[(addr >> 2)];
      // printf("Loading at %x : %x\n", addr >> 2, mergedAccess);
      if (!pendingWrite && opType == STORE && mask != WORD) {
        waitOut      = true;
        valueLoaded  = mergedAccess;
        pendingWrite = 1;
      } else {
        pendingWrite                 = 0;
        waitOut                      = false;
        ac_int<32, false> dataOutTmp = mergedAccess;
        switch (mask) {
          case BYTE:
            t8  = dataOutTmp.slc<8>(((int)addr.slc<2>(0)) << 3);
            bit = t8.slc<1>(7);
            dataOut.set_slc(0, t8);
            dataOut.set_slc(8, (ac_int<24, true>)bit);
            break;
          case HALF:
            t16 = dataOutTmp.slc<16>(addr[1] ? 16 : 0);
            bit = t16.slc<1>(15);
            dataOut.set_slc(0, t16);
            dataOut.set_slc(16, (ac_int<16, true>)bit);
            break;
          case WORD:
            dataOut = dataOutTmp;
            break;
          case BYTE_U:
            dataOut = dataOutTmp.slc<8>(((int)addr.slc<2>(0)) << 3) & 0xff;
            break;
          case HALF_U:
            dataOut = dataOutTmp.slc<16>(addr[1] ? 16 : 0) & 0xffff;
            break;
        }
      }

    } else if (opType == STORE) {
      pendingWrite = 0;
      // no latency, wait is always set to false
      waitOut                      = false;
      ac_int<32, false> valToStore = 0;
      switch (mask) {
        case BYTE_U:
        case BYTE:
          valToStore = valueLoaded;
          valToStore.set_slc(((int)addr.slc<2>(0)) << 3, dataIn.template slc<8>(0));
          break;
        case HALF:
        case HALF_U:
          valToStore = valueLoaded;
          valToStore.set_slc(addr[1] ? 16 : 0, dataIn.template slc<16>(0));
          break;
        case WORD:
          valToStore = dataIn;
          break;
      }
      // printf("Loading at %x : %x\n", addr >> 2, mergedAccess);
      data[(addr >> 2)] = valToStore;
    }
  }
};

template <unsigned int INTERFACE_SIZE> class SimpleMemory : public MemoryInterface<INTERFACE_SIZE> {
public:
  ac_int<32, false>* data;

  SimpleMemory(ac_int<32, false>* arg) { data = arg; }
  void process(const ac_int<32, false> addr, const memMask mask, const memOpType opType, const ac_int<INTERFACE_SIZE * 8, false> dataIn,
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
          /*case LONG:
            for (int oneWord = 0; oneWord < INTERFACE_SIZE / 4; oneWord++)
              data[(addr >> 2) + oneWord] = dataIn.template slc<32>(32 * oneWord);
            break;*/
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
          /*case LONG:
            for (int oneWord = 0; oneWord < INTERFACE_SIZE / 4; oneWord++)
              dataOut.set_slc(32 * oneWord, data[(addr >> 2) + oneWord]);
            break;*/
          case BYTE_U:
            dataOut = data[addr >> 2].slc<8>(((int)addr.slc<2>(0)) << 3) & 0xff;
            break;
          case HALF_U:
            dataOut = data[addr >> 2].slc<16>(addr[1] ? 16 : 0) & 0xffff;
            break;
        }
        break;

      default: // case NONE
        break;
    }
    waitOut = false;
  }
};


template <unsigned int INTERFACE_SIZE> class DirectMemory  {
public:

  DirectMemory() { }
  void process( unsigned int /*ac_int<32, false>*/ data [1<<24], const ac_int<32, false> addr, const memMask mask, const memOpType opType, const ac_int<INTERFACE_SIZE * 8, false> dataIn,
               ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut , volatile unsigned int * out1 )
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
            //data[addr >> 2].set_slc(((int)addr.slc<2>(0)) << 3, dataIn.template slc<8>(0));
            temp.set_slc(((int)addr.slc<2>(0)) << 3, dataIn.template slc<8>(0));
            data[addr >> 2] = temp;
            break;
          case HALF:
          case HALF_U:
            temp = data[addr >> 2];
            //data[addr >> 2].set_slc(addr[1] ? 16 : 0, dataIn.template slc<16>(0));
            temp.set_slc(addr[1] ? 16 : 0, dataIn.template slc<16>(0));
            data[addr >> 2] = temp;
            break;
          case WORD:
            if( (addr>>2) == 0x40000 )
            {
              *out1 =  dataIn;
           //    printf ("out = %u \n ",*out1);
            }
            else
                data[addr >> 2] = dataIn;
            break;
          /*case LONG:
            data[addr >> 2] = dataIn;*/
            /*for (int oneWord = 0; oneWord < INTERFACE_SIZE / 4; oneWord++)
              data[(addr >> 2) + oneWord] = dataIn.template slc<32>(32 * oneWord);*/
           /* break;*/
        }
        break;
      case LOAD:
        switch (mask) {
          case BYTE:
            temp = data[addr >> 2];
            t8  = temp.slc<8>(((int)addr.slc<2>(0)) << 3);
            bit = t8.slc<1>(7);
            dataOut.set_slc(0, t8);
            dataOut.set_slc(8, (ac_int<24, true>)bit);
            break;
          case HALF:
            temp = data[addr >> 2];
            t16 = temp.slc<16>(addr[1] ? 16 : 0);
            bit = t16.slc<1>(15);
            dataOut.set_slc(0, t16);
            dataOut.set_slc(16, (ac_int<16, true>)bit);
            break;
          case WORD:
            dataOut = data[addr >> 2];
            break;
          /*case LONG:
            dataOut = data[addr >> 2];

            //for (int oneWord = 0; oneWord < INTERFACE_SIZE / 4; oneWord++)
            //  dataOut.set_slc(32 * oneWord, data[(addr >> 2) + oneWord]);
            break;*/
          case BYTE_U:
            temp = data[addr >> 2];
            dataOut = temp.slc<8>(((int)addr.slc<2>(0)) << 3) & 0xff;
            break;
          case HALF_U:
            temp = data[addr >> 2];
            dataOut = temp.slc<16>(addr[1] ? 16 : 0) & 0xffff;
            break;
        }
        break;

      default: // case NONE
        break;
    }
    waitOut = false;
  }
};



template <unsigned int INTERFACE_SIZE> class DirectLoadMemory  {
public:

  DirectLoadMemory() { }
  void process( unsigned int /*ac_int<32, false>*/ data [1<<24], const ac_int<32, false> addr, const memMask mask, const memOpType opType, const ac_int<INTERFACE_SIZE * 8, false> dataIn,
               ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut)
  {
    // no latency, wait is always set to false

    ac_int<32, true> temp;
    ac_int<8, false> t8;
    ac_int<1, true> bit;
    ac_int<16, false> t16;

    switch (opType) {
      case LOAD:
        switch (mask) {
          case BYTE:
            temp = data[addr >> 2];
            t8  = temp.slc<8>(((int)addr.slc<2>(0)) << 3);
            bit = t8.slc<1>(7);
            dataOut.set_slc(0, t8);
            dataOut.set_slc(8, (ac_int<24, true>)bit);
            break;
          case HALF:
            temp = data[addr >> 2];
            t16 = temp.slc<16>(addr[1] ? 16 : 0);
            bit = t16.slc<1>(15);
            dataOut.set_slc(0, t16);
            dataOut.set_slc(16, (ac_int<16, true>)bit);
            break;
          case WORD:
            dataOut = data[addr >> 2];
            break;
          /*case LONG:
            dataOut = data[addr >> 2];

            //for (int oneWord = 0; oneWord < INTERFACE_SIZE / 4; oneWord++)
            //  dataOut.set_slc(32 * oneWord, data[(addr >> 2) + oneWord]);
            break;*/
          case BYTE_U:
            temp = data[addr >> 2];
            dataOut = temp.slc<8>(((int)addr.slc<2>(0)) << 3) & 0xff;
            break;
          case HALF_U:
            temp = data[addr >> 2];
            dataOut = temp.slc<16>(addr[1] ? 16 : 0) & 0xffff;
            break;
        }
        break;

      default: // case NONE
        break;
    }
    waitOut = false;
  }
};

#endif //__MEMORY_INTERFACE_H__
