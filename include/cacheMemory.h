/*
 * cacheMemory.h
 *
 *  Created on: 15 févr. 2019
 *      Author: simon
 */

#ifndef INCLUDE_CACHEMEMORY_H_
#define INCLUDE_CACHEMEMORY_H_

#include <ac_int.h>
#include <memoryInterface.h>

#define LINE_SIZE 16
#define LOG_LINE_SIZE 4

#define SET_SIZE 64
#define LOG_SET_SIZE 6

#define ASSOCIATIVITY 4
#define LOG_ASSOCIATIVITY 2

#define TAG_SIZE 32 - LOG_LINE_SIZE - LOG_LINE_SIZE

/************************************************************************
 * 	Following values are templates:
 * 		- OFFSET_SIZE
 * 		- TAG_SIZE
 * 		- SET_SIZE
 * 		- ASSOCIATIVITY
 ************************************************************************/
// template<int OFFSET_SIZE, int TAG_SIZE, int SET_SIZE, int ASSOCIATIVITY>
class CacheMemory : public MemoryInterface {
public:
#ifdef __HLS__
  IncompleteMemory* nextLevel;
#else
  MemoryInterface* nextLevel;
#endif

  ac_int<TAG_SIZE + LINE_SIZE * 8, false> cacheMemory[SET_SIZE][ASSOCIATIVITY];
  ac_int<40, false> age[SET_SIZE][ASSOCIATIVITY];
  ac_int<1, false> dataValid[SET_SIZE][ASSOCIATIVITY];

  ac_int<4, false> cacheState;                // Used for the internal state machine
  ac_int<LOG_ASSOCIATIVITY, false> older = 0; // Set where the miss occurs

  // Variables for next level access
  ac_int<LINE_SIZE * 8 + TAG_SIZE, false> newVal, oldVal;
  ac_int<32, false> nextLevelAddr;
  memOpType nextLevelOpType;
  ac_int<32, false> nextLevelDataIn;
  ac_int<32, false> nextLevelDataOut;
  ac_int<40, false> cycle;
  ac_int<LOG_ASSOCIATIVITY, false> setMiss;
  bool isValid;

  bool wasStore = false;
  ac_int<LOG_ASSOCIATIVITY, false> setStore;
  ac_int<LOG_SET_SIZE, false> placeStore;
  ac_int<LINE_SIZE * 8 + TAG_SIZE, false> valStore;
  ac_int<32, false> dataOutStore;

  bool nextLevelWaitOut;

  bool VERBOSE = false;

  // Stats
  unsigned long numberAccess, numberMiss;

#ifdef __HLS__
  CacheMemory(IncompleteMemory* nextLevel, bool v){
#else
  CacheMemory(MemoryInterface* nextLevel, bool v)
  {
#endif
      this->nextLevel = nextLevel;
  for (int oneSetElement = 0; oneSetElement < SET_SIZE; oneSetElement++) {
    for (int oneSet = 0; oneSet < ASSOCIATIVITY; oneSet++) {
      cacheMemory[oneSetElement][oneSet] = 0;
      age[oneSetElement][oneSet]         = 0;
      dataValid[oneSetElement][oneSet]   = 0;
    }
  }
  VERBOSE      = v;
  numberAccess = 0;
  numberMiss   = 0;
}

	void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn, ac_int<32, false>& dataOut, bool& waitOut)
{

  ac_int<LOG_SET_SIZE, false> place = addr.slc<LOG_SET_SIZE>(LOG_LINE_SIZE); // bit size is the log(setSize)
  ac_int<TAG_SIZE, false> tag =
      addr.slc<TAG_SIZE>(LOG_LINE_SIZE + LOG_SET_SIZE); // startAddress is log(lineSize) + log(setSize) + 2
  ac_int<LOG_LINE_SIZE, false> offset = addr.slc<LOG_LINE_SIZE - 2>(2); // bitSize is log(lineSize), start address
                                                                        // is 2(because of #bytes in a word)

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

        //				fprintf(stdout, "Reading at addr %x\n",
        // addr);

        ac_int<TAG_SIZE, false> tag1 = val1.slc<TAG_SIZE>(0);
        ac_int<TAG_SIZE, false> tag2 = val2.slc<TAG_SIZE>(0);
        ac_int<TAG_SIZE, false> tag3 = val3.slc<TAG_SIZE>(0);
        ac_int<TAG_SIZE, false> tag4 = val4.slc<TAG_SIZE>(0);

        bool hit1 = (tag1 == tag) && valid1;
        bool hit2 = (tag2 == tag) && valid2;
        bool hit3 = (tag3 == tag) && valid3;
        bool hit4 = (tag4 == tag) && valid4;

        bool hit                             = hit1 | hit2 | hit3 | hit4;
        ac_int<LOG_ASSOCIATIVITY, false> set = 0;
        ac_int<LINE_SIZE * 8, false> selectedValue;
        ac_int<TAG_SIZE, false> tag;

        if (hit1) {
          selectedValue = val1.slc<LINE_SIZE * 8>(TAG_SIZE);
          tag           = tag1;
          set           = 0;
        }

        if (hit2) {
          selectedValue = val2.slc<LINE_SIZE * 8>(TAG_SIZE);
          tag           = tag2;
          set           = 1;
        }

        if (hit3) {
          selectedValue = val3.slc<LINE_SIZE * 8>(TAG_SIZE);
          tag           = tag3;
          set           = 2;
        }

        if (hit4) {
          selectedValue = val4.slc<LINE_SIZE * 8>(TAG_SIZE);
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
                localValStore.set_slc((((int)addr.slc<2>(0)) << 3) + TAG_SIZE + 4 * 8 * offset, dataIn.slc<8>(0));
                break;
              case HALF:
                localValStore.set_slc((addr[1] ? 16 : 0) + TAG_SIZE + 4 * 8 * offset, dataIn.slc<16>(0));
                break;
              case WORD:
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
                signedByte = selectedValue.slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset);
                signedWord = signedByte;
                dataOut.set_slc(0, signedWord);
                break;
              case HALF:
                signedHalf = selectedValue.slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset);
                signedWord = signedHalf;
                dataOut.set_slc(0, signedWord);
                break;
              case WORD:
                dataOut = selectedValue.slc<32>(4 * 8 * offset);
                break;
              case BYTE_U:
                dataOut = selectedValue.slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset) & 0xff;
                break;
              case HALF_U:
                dataOut = selectedValue.slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset) & 0xffff;
                break;
            }
          }
          // age[place][set] = cycle;

        } else {
          numberMiss++;
          cacheState = 10;
        }
      } else {

        if (cacheState == 10) {
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
        }

        ac_int<32, false> oldAddress =
            (((int)oldVal.slc<TAG_SIZE>(0)) << (LOG_LINE_SIZE + LOG_SET_SIZE)) | (((int)place) << LOG_LINE_SIZE);
        // First we write back the four memory values in upper level

        if (cacheState >= 7) { // Then we read four values from upper level
          nextLevelAddr   = oldAddress + ((cacheState - 7) << 2);
          nextLevelDataIn = oldVal.slc<32>((cacheState - 7) * 4 * 8 + TAG_SIZE);
          nextLevelOpType = (isValid) ? STORE : NONE;
        } else if (cacheState >= 2) { // Then we read four values from upper level
          if (cacheState != 6) {
            newVal.set_slc((cacheState - 2) * 4 * 8 + TAG_SIZE,
                           nextLevelDataOut); // at addr +1
          }

          if (cacheState != 2) {
            nextLevelAddr =
                (((int)addr.slc<32 - LOG_LINE_SIZE>(LOG_LINE_SIZE)) << LOG_LINE_SIZE) + ((cacheState - 3) << 2);
            nextLevelOpType = LOAD;
          }
        }

        cacheState--;

        if (cacheState == 1) {
          if (opType == STORE) {
            switch (mask) {
              case BYTE:
                newVal.set_slc((((int)addr.slc<2>(0)) << 3) + TAG_SIZE + 4 * 8 * offset, dataIn.slc<8>(0));
                break;
              case HALF:
                newVal.set_slc((addr[1] ? 16 : 0) + TAG_SIZE + 4 * 8 * offset, dataIn.slc<16>(0));
                break;
              case WORD:
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
              signedByte = newVal.slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset + TAG_SIZE);
              signedWord = signedByte;
              dataOut.set_slc(0, signedWord);
              break;
            case HALF:
              signedHalf = newVal.slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset + TAG_SIZE);
              signedWord = signedHalf;
              dataOut.set_slc(0, signedWord);
              break;
            case WORD:
              dataOut = newVal.slc<32>(4 * 8 * offset + TAG_SIZE);
              break;
            case BYTE_U:
              dataOut = newVal.slc<8>((((int)addr.slc<2>(0)) << 3) + 4 * 8 * offset + TAG_SIZE) & 0xff;
              break;
            case HALF_U:
              dataOut = newVal.slc<16>((addr[1] ? 16 : 0) + 4 * 8 * offset + TAG_SIZE) & 0xffff;
              break;
          }
          dataOutStore = dataOut;
        }
      }
    }
  }

  this->nextLevel->process(nextLevelAddr, WORD, nextLevelOpType, nextLevelDataIn, nextLevelDataOut, nextLevelWaitOut);
  waitOut = nextLevelWaitOut || cacheState || (wasStore && opType != NONE);
}
}
;

#endif /* INCLUDE_CACHEMEMORY_H_ */
