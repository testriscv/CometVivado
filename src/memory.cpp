/*
 * memory.cpp
 *
 *  Created on: 25 janv. 2019
 *      Author: simon
 */

#include <memory.h>
#include <cacheMemory.h>

#define OFFSET_SIZE 4
#define TAG_SIZE 32-3-4
#define ASSOCIATIVITY 4
#define SET_SIZE 256

ac_int<128+TAG_SIZE, false> data_set1[256];
ac_int<128+TAG_SIZE, false> data_set2[256];
ac_int<128+TAG_SIZE, false> data_set3[256];
ac_int<128+TAG_SIZE, false> data_set4[256];

ac_int<32, false> ages[256*4];

ac_int<3, false> miss;
ac_int<128+TAG_SIZE, false> newVal;


void CacheMemory::process(ac_int<32, false> addr,
		memMask mask,
		memOpType opType,
		ac_int<32, false> dataIn,
		ac_int<32, false>& dataOut,
		bool& waitOut){

	ac_int<3, false> place = addr.slc<3>(4);
	ac_int<TAG_SIZE, false> tag = addr.slc<TAG_SIZE>(7);
	ac_int<4, false> offset = addr.slc<4>(0);

	if (miss == 0){


		ac_int<128+TAG_SIZE, false> val1 = data_set1[place];
		ac_int<128+TAG_SIZE, false> val2 = data_set2[place];
		ac_int<128+TAG_SIZE, false> val3 = data_set3[place];
		ac_int<128+TAG_SIZE, false> val4 = data_set4[place];

		ac_int<TAG_SIZE, false> tag1 = val1.slc<TAG_SIZE>(0);
		ac_int<TAG_SIZE, false> tag2 = val2.slc<TAG_SIZE>(0);
		ac_int<TAG_SIZE, false> tag3 = val3.slc<TAG_SIZE>(0);
		ac_int<TAG_SIZE, false> tag4 = val4.slc<TAG_SIZE>(0);

		bool hit1 = (tag1 == tag);
		bool hit2 = (tag2 == tag);
		bool hit3 = (tag3 == tag);
		bool hit4 = (tag4 == tag);

		bool hit = hit1 | hit2 | hit3 | hit4;
		ac_int<128, false> value;

		if (hit1)
			value = val1.slc<128>(TAG_SIZE);
		if (hit2)
			value = val2.slc<128>(TAG_SIZE);
		if (hit3)
			value = val3.slc<128>(TAG_SIZE);
		if (hit4)
			value = val4.slc<128>(TAG_SIZE);

		if (hit){
			dataOut = value.slc<32>(8*offset);
		}
		else{
		    miss = 5;
		    stall = 1;
		}
		}
		else{

	        if (miss == 5)
			  ac_int<128+TAG_SIZE, false> newVal = tag;
			//CORE_UINT(32) age1 = ages[4*offset+0];
			//CORE_UINT(32) age2 = ages[4*offset+1];
			//CORE_UINT(32) age3 = ages[4*offset+2];
			//CORE_UINT(32) age4 = ages[4*offset+3];

	        if (miss >= 2)
			  newVal.set_slc((miss-2)*8+TAG_SIZE, externalMemory[(address>>2) + (miss-2)]);

	        miss--;
			//if (age1<age2 & age1<age3 & age1<age4)
			if (miss == 1){
				data_set1[place] = newVal;

			      int newOffset = TAG_SIZE + 8*offset;
			      dataOut = newVal.slc<32>(newOffset);

	        }

	        if (miss == 0){
			      stall = 0;
	        }
		//	if (age2<age1 & age2<age3 & age2<age4)
			//	data_set2[place] = newVal;

		//	if (age3<age1 & age3<age2 & age3<age4)
		//		data_set3[place] = newVal;

		//	if (age4<age1 & age4<age2 & age4<age3)
		//		data_set4[place] = newVal;




		}

}

