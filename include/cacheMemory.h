/*
 * cacheMemory.h
 *
 *  Created on: 15 févr. 2019
 *      Author: simon
 */

#ifndef INCLUDE_CACHEMEMORY_H_
#define INCLUDE_CACHEMEMORY_H_

#include "logarithm.h"
#include "memoryInterface.h"
#include <ac_int.h>

/************************************************************************
 * 	Following values are templates:
 * 		- OFFSET_SIZE
 * 		- TAG_SIZE
 * 		- SET_SIZE
 * 		- ASSOCIATIVITY
 ************************************************************************/
template <unsigned int INTERFACE_SIZE, int LINE_SIZE, int SET_SIZE>
class CacheMemory : public MemoryInterface<INTERFACE_SIZE> {

  static const int LOG_SET_SIZE           = log2const<SET_SIZE>::value;
  static const int LOG_LINE_SIZE          = log2const<LINE_SIZE>::value;
  static const int TAG_SIZE               = (32 - LOG_LINE_SIZE - LOG_SET_SIZE);
  static const int ASSOCIATIVITY          = 4;
  static const int LOG_ASSOCIATIVITY      = 2;
  static const int STATE_CACHE_MISS       = ((LINE_SIZE / INTERFACE_SIZE) * 2 + 2);
  static const int STATE_CACHE_LAST_STORE = ((LINE_SIZE / INTERFACE_SIZE) + 3);
  static const int STATE_CACHE_FIRST_LOAD = ((LINE_SIZE / INTERFACE_SIZE) + 2);
  static const int STATE_CACHE_LAST_LOAD  = 2;
  static const int LOG_INTERFACE_SIZE     = log2const<INTERFACE_SIZE>::value;

public:
  MemoryInterface<INTERFACE_SIZE>* nextLevel;

  ac_int<TAG_SIZE + LINE_SIZE * 8, false> cacheMemory[SET_SIZE][ASSOCIATIVITY];
  ac_int<40, false> age[SET_SIZE][ASSOCIATIVITY];
  ac_int<1, false> dataValid[SET_SIZE][ASSOCIATIVITY];

  ac_int<6, false> cacheState;                // Used for the internal state machine
  ac_int<LOG_ASSOCIATIVITY, false> older = 0; // Set where the miss occurs

  // Variables for next level access
  ac_int<LINE_SIZE * 8 + TAG_SIZE, false> newVal, oldVal;
  ac_int<32, false> nextLevelAddr;
  memOpType nextLevelOpType;
  ac_int<INTERFACE_SIZE * 8, false> nextLevelDataIn;
  ac_int<INTERFACE_SIZE * 8, false> nextLevelDataOut;
  ac_int<40, false> cycle;
  ac_int<LOG_ASSOCIATIVITY, false> setMiss;
  bool isValid;

  bool wasStore = false;
  ac_int<LOG_ASSOCIATIVITY, false> setStore;
  ac_int<LOG_SET_SIZE, false> placeStore;
  ac_int<LINE_SIZE * 8 + TAG_SIZE, false> valStore;
  ac_int<INTERFACE_SIZE * 8, false> dataOutStore;

  bool nextLevelWaitOut;

  bool VERBOSE = false;

  // Stats
  unsigned long numberAccess, numberMiss;

  CacheMemory(MemoryInterface<INTERFACE_SIZE>* nextLevel, bool v)
  {
    this->nextLevel = nextLevel;
    for (int oneSetElement = 0; oneSetElement < SET_SIZE; oneSetElement++) {
      for (int oneSet = 0; oneSet < ASSOCIATIVITY; oneSet++) {
        cacheMemory[oneSetElement][oneSet] = 0;
        age[oneSetElement][oneSet]         = 0;
        dataValid[oneSetElement][oneSet]   = 0;
      }
    }
    VERBOSE          = v;
    numberAccess     = 0;
    numberMiss       = 0;
    nextLevelWaitOut = false;
    wasStore         = false;
    cacheState       = 0;
    nextLevelOpType  = NONE;
  }

  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<INTERFACE_SIZE * 8, false> dataIn,
               ac_int<INTERFACE_SIZE * 8, false>& dataOut, bool& waitOut)
  {

    // bit size is the log(setSize)
    ac_int<LOG_SET_SIZE, false> place = addr.slc<LOG_SET_SIZE>(LOG_LINE_SIZE);
    // startAddress is log(lineSize) + log(setSize) + 2
    ac_int<TAG_SIZE, false> tag = addr.slc<TAG_SIZE>(LOG_LINE_SIZE + LOG_SET_SIZE);
    // bitSize is log(lineSize), start address is 2(because of #bytes in a word)
    ac_int<LOG_LINE_SIZE, false> offset = addr.slc<LOG_LINE_SIZE - 2>(2);

    if (!nextLevelWaitOut) {
      cycle++;

      if (wasStore || cacheState == 1) {

        cacheMemory[placeStore][setStore] = valStore;
        age[placeStore][setStore]         = cycle;
        dataValid[placeStore][setStore]   = 1;
        dataOut                           = dataOutStore;
        wasStore                          = false;
        cacheState                        = 0;

      } else if (opType != NONE) {

        ac_int<LINE_SIZE * 8 + TAG_SIZE, false> val1 = cacheMemory[place][0];
        ac_int<LINE_SIZE * 8 + TAG_SIZE, false> val2 = cacheMemory[place][1];
        ac_int<LINE_SIZE * 8 + TAG_SIZE, false> val3 = cacheMemory[place][2];
        ac_int<LINE_SIZE * 8 + TAG_SIZE, false> val4 = cacheMemory[place][3];

        ac_int<1, false> valid1 = dataValid[place][0];
        ac_int<1, false> valid2 = dataValid[place][1];
        ac_int<1, false> valid3 = dataValid[place][2];
        ac_int<1, false> valid4 = dataValid[place][3];

        ac_int<16, false> age1 = age[place][0];
        ac_int<16, false> age2 = age[place][1];
        ac_int<16, false> age3 = age[place][2];
        ac_int<16, false> age4 = age[place][3];

        if (cacheState == 0) {
          numberAccess++;

          ac_int<TAG_SIZE, false> tag1 = val1.template slc<TAG_SIZE>(0);
          ac_int<TAG_SIZE, false> tag2 = val2.template slc<TAG_SIZE>(0);
          ac_int<TAG_SIZE, false> tag3 = val3.template slc<TAG_SIZE>(0);
          ac_int<TAG_SIZE, false> tag4 = val4.template slc<TAG_SIZE>(0);

          bool hit1 = (tag1 == tag) && valid1;
          bool hit2 = (tag2 == tag) && valid2;
          bool hit3 = (tag3 == tag) && valid3;
          bool hit4 = (tag4 == tag) && valid4;
          bool hit  = hit1 | hit2 | hit3 | hit4;

          ac_int<LOG_ASSOCIATIVITY, false> set = 0;
          ac_int<LINE_SIZE * 8, false> selectedValue;
          ac_int<TAG_SIZE, false> tag;

          if (hit1) {
            selectedValue = val1.template slc<LINE_SIZE * 8>(TAG_SIZE);
            tag           = tag1;
            set           = 0;
          }

          if (hit2) {
            selectedValue = val2.template slc<LINE_SIZE * 8>(TAG_SIZE);
            tag           = tag2;
            set           = 1;
          }

          if (hit3) {
            selectedValue = val3.template slc<LINE_SIZE * 8>(TAG_SIZE);
            tag           = tag3;
            set           = 2;
          }

          if (hit4) {
            selectedValue = val4.template slc<LINE_SIZE * 8>(TAG_SIZE);
            tag           = tag4;
            set           = 3;
          }

          ac_int<8, true> signedByte;
          ac_int<16, true> signedHalf;
          ac_int<32, true> signedWord;

          if (hit) {
            ac_int<LINE_SIZE * 8 + TAG_SIZE, false> localValStore = 0;
            localValStore.set_slc(TAG_SIZE, selectedValue);
            localValStore.set_slc(0, tag);

            // First we handle the store
            if (opType == STORE) {
              switch (mask) {
                case BYTE:
                case BYTE_U:
                  localValStore.set_slc((((int)addr.slc<2>(0)) << 3) + TAG_SIZE + 4 * 8 * offset,
                                        dataIn.template slc<8>(0));
                  break;
                case HALF:
                case HALF_U:
                  localValStore.set_slc((addr[1] ? 16 : 0) + TAG_SIZE + 4 * 8 * offset, dataIn.template slc<16>(0));
                  break;
                case WORD:
                  localValStore.set_slc(TAG_SIZE + 4 * 8 * offset, dataIn.template slc<32>(0));
                  break;
                case LONG:
                  localValStore.set_slc(TAG_SIZE + 4 * 8 * offset, dataIn);
                  break;
              }

              placeStore = place;
              setStore   = set;
              valStore   = localValStore;
              wasStore   = true;

            } else {
              switch (mask) {
                case BYTE:
                  signedByte = selectedValue.template slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset);
                  signedWord = signedByte;
                  dataOut.set_slc(0, signedWord);
                  break;
                case HALF:
                  signedHalf = selectedValue.template slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset);
                  signedWord = signedHalf;
                  dataOut.set_slc(0, signedWord);
                  break;
                case WORD:
                  dataOut = selectedValue.template slc<32>(4 * 8 * offset);
                  break;
                case BYTE_U:
                  dataOut = selectedValue.template slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset) & 0xff;
                  break;
                case HALF_U:
                  dataOut = selectedValue.template slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset) & 0xffff;
                  break;
                case LONG:
                  dataOut = selectedValue.template slc<INTERFACE_SIZE * 8>(4 * 8 * offset);
                  break;
              }

              // printf("Hit read %x at %x\n", (unsigned int)dataOut.slc<32>(0), (unsigned int)addr);
            }
            // age[place][set] = cycle;

          } else {
            numberMiss++;
            cacheState = STATE_CACHE_MISS;
          }
        } else {
          // printf("Miss %d\n", (unsigned int)cacheState);

          if (cacheState == STATE_CACHE_MISS) {
            newVal  = tag;
            setMiss = (age1 < age2 && age1 < age3 && age1 < age4)
                          ? 0
                          : ((age2 < age1 && age2 < age3 && age2 < age4)
                                 ? 1
                                 : ((age3 < age2 && age3 < age1 && age3 < age4) ? 2 : 3));
            oldVal = (age1 < age2 && age1 < age3 && age1 < age4)
                         ? val1
                         : ((age2 < age1 && age2 < age3 && age2 < age4)
                                ? val2
                                : ((age3 < age2 && age3 < age1 && age3 < age4) ? val3 : val4));
            isValid = (age1 < age2 && age1 < age3 && age1 < age4)
                          ? valid1
                          : ((age2 < age1 && age2 < age3 && age2 < age4)
                                 ? valid2
                                 : ((age3 < age2 && age3 < age1 && age3 < age4) ? valid3 : valid4));
            // printf("TAG is %x\n", oldVal.slc<TAG_SIZE>(0));
          }

          ac_int<32, false> oldAddress = (((int)oldVal.template slc<TAG_SIZE>(0)) << (LOG_LINE_SIZE + LOG_SET_SIZE)) |
                                         (((int)place) << LOG_LINE_SIZE);
          // First we write back the four memory values in upper level

          if (cacheState >= STATE_CACHE_LAST_STORE) {
            // We store all values into next memory interface
            nextLevelAddr   = oldAddress + (((int)(cacheState - STATE_CACHE_LAST_STORE)) << LOG_INTERFACE_SIZE);
            nextLevelDataIn = oldVal.template slc<INTERFACE_SIZE * 8>(
                (cacheState - STATE_CACHE_LAST_STORE) * INTERFACE_SIZE * 8 + TAG_SIZE);
            nextLevelOpType = (isValid) ? STORE : NONE;

            // printf("Writing back %x %x at %x\n", (unsigned int)nextLevelDataIn.slc<32>(0),
            //        (unsigned int)nextLevelDataIn.slc<32>(32), (unsigned int)nextLevelAddr);

          } else if (cacheState >= STATE_CACHE_LAST_LOAD) {
            // Then we read values from next memory level
            if (cacheState != STATE_CACHE_FIRST_LOAD) {
              newVal.set_slc(((unsigned int)(cacheState - STATE_CACHE_LAST_LOAD)) * INTERFACE_SIZE * 8 + TAG_SIZE,
                             nextLevelDataOut); // at addr +1
            }

            if (cacheState != STATE_CACHE_LAST_LOAD) {
              // We initiate the load at the address determined by next cache state
              nextLevelAddr = (((int)addr.slc<32 - LOG_LINE_SIZE>(LOG_LINE_SIZE)) << LOG_LINE_SIZE) +
                              ((cacheState - STATE_CACHE_LAST_LOAD - 1) << LOG_INTERFACE_SIZE);
              nextLevelOpType = LOAD;
            }
          }

          cacheState--;

          if (cacheState == 1) {
            if (opType == STORE) {
              switch (mask) {
                case BYTE:
                case BYTE_U:
                  newVal.set_slc((((int)addr.slc<2>(0)) << 3) + TAG_SIZE + 4 * 8 * offset, dataIn.template slc<8>(0));
                  break;
                case HALF:
                case HALF_U:
                  newVal.set_slc((addr[1] ? 16 : 0) + TAG_SIZE + 4 * 8 * offset, dataIn.template slc<16>(0));
                  break;
                case WORD:
                  newVal.set_slc(TAG_SIZE + 4 * 8 * offset, dataIn.template slc<32>(0));
                  break;
                case LONG:
                  newVal.set_slc(TAG_SIZE + 4 * 8 * offset, dataIn);
                  break;
              }
            }

            placeStore = place;
            setStore   = setMiss;
            valStore   = newVal;

            // cacheMemory[place][setMiss] = newVal;
            // dataValid[place][setMiss] = 1;
            // age[place][setMiss] = cycle;
            nextLevelOpType = NONE;

            ac_int<8, true> signedByte;
            ac_int<16, true> signedHalf;
            ac_int<32, true> signedWord;

            switch (mask) {
              case BYTE:
                signedByte = newVal.template slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset + TAG_SIZE);
                signedWord = signedByte;
                dataOut.set_slc(0, signedWord);
                break;
              case HALF:
                signedHalf = newVal.template slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset + TAG_SIZE);
                signedWord = signedHalf;
                dataOut.set_slc(0, signedWord);
                break;
              case WORD:
                dataOut = newVal.template slc<32>(4 * 8 * offset + TAG_SIZE);
                break;
              case BYTE_U:
                dataOut = newVal.template slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset + TAG_SIZE) & 0xff;
                break;
              case HALF_U:
                dataOut = newVal.template slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset + TAG_SIZE) & 0xffff;
                break;
              case LONG:
                dataOut = newVal.template slc<INTERFACE_SIZE * 8>(4 * 8 * offset);
                break;
            }
            // printf("After Miss read %x at %x\n", (unsigned int)dataOut.slc<32>(0), (unsigned int)addr);

            dataOutStore = dataOut;
          }
        }
      }
    }

    this->nextLevel->process(nextLevelAddr, LONG, nextLevelOpType, nextLevelDataIn, nextLevelDataOut, nextLevelWaitOut);
    waitOut = nextLevelWaitOut || cacheState || (wasStore && opType != NONE);
  }
};

#endif /* INCLUDE_CACHEMEMORY_H_ */
