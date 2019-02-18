/*
 * cacheMemory.h
 *
 *  Created on: 15 févr. 2019
 *      Author: simon
 */

#ifndef INCLUDE_CACHEMEMORY_H_
#define INCLUDE_CACHEMEMORY_H_

#include "memoryInterface.h"
#include "memory.h"


/************************************************************************
 * 	Following values are templates:
 * 		- OFFSET_SIZE
 * 		- TAG_SIZE
 * 		- SET_SIZE
 * 		- ASSOCIATIVITY
 ************************************************************************/
template<int OFFSET_SIZE, int TAG_SIZE, int SET_SIZE, int ASSOCIATIVITY>

class CacheMemory: public MemoryInterface {
protected:
  MemoryInterface *nextLevel;
  ac_int<TAG_SIZE+SET_SIZE, false> cacheMemory[SET_SIZE][ASSOCIATIVITY];
  ac_int<16, false> age[SET_SIZE][ASSOCIATIVITY];

  ac_int<4, false> cacheState; //Used for the internal state machine


public:

  CacheMemory(MemoryInterface *nextLevel){
	  nextLevel = nextLevel;
	  for (int oneSetElement = 0; oneSetElement<SET_SIZE; oneSetElement++){
		  for (int oneSet = 0; oneSet < ASSOCIATIVITY; oneSet++){
			  cacheMemory[oneSetElement][oneSet] = 0;
			  age[oneSetElement][oneSet] = 0;
		  }
	  }
  }

  void process(ac_int<32, false> addr, memMask mask, memOpType opType, ac_int<32, false> dataIn, ac_int<32, false>& dataOut, bool& waitOut);
};


#endif /* INCLUDE_CACHEMEMORY_H_ */
