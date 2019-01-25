/*
 * memory.cpp
 *
 *  Created on: 25 janv. 2019
 *      Author: simon
 */

#include <portability.h>

#define OFFSET_SIZE 4
#define TAG_SIZE 32-3-4

#define SET_SIZE 256

CORE_UINT(128+TAG_SIZE) data_set1[256];
CORE_UINT(128+TAG_SIZE) data_set2[256];
CORE_UINT(128+TAG_SIZE) data_set3[256];
CORE_UINT(128+TAG_SIZE) data_set4[256];

CORE_UINT(32) ages[256*4];

CORE_UINT(3) miss;
CORE_UINT(128+TAG_SIZE) newVal;

void readMemory(CORE_INT(32) address, CORE_UINT(4) byteEnable, CORE_UINT(32) externalMemory[65536], CORE_UINT(32) &dataOut, CORE_UINT(1) &stall){

    while (1) {
	CORE_UINT(3) place = address.SLC(3, 4);
	CORE_UINT(TAG_SIZE) tag = address.SLC(TAG_SIZE, 7);
	CORE_UINT(4) offset = address.SLC(4, 0);

    if (miss == 0){


	CORE_UINT(128+TAG_SIZE) val1 = data_set1[place];
	CORE_UINT(128+TAG_SIZE) val2 = data_set2[place];
	CORE_UINT(128+TAG_SIZE) val3 = data_set3[place];
	CORE_UINT(128+TAG_SIZE) val4 = data_set4[place];

	CORE_UINT(TAG_SIZE) tag1 = val1.SLC(TAG_SIZE, 0);
	CORE_UINT(TAG_SIZE) tag2 = val2.SLC(TAG_SIZE, 0);
	CORE_UINT(TAG_SIZE) tag3 = val3.SLC(TAG_SIZE, 0);
	CORE_UINT(TAG_SIZE) tag4 = val4.SLC(TAG_SIZE, 0);

	CORE_UINT(1) hit1 = (tag1 == tag);
	CORE_UINT(1) hit2 = (tag2 == tag);
	CORE_UINT(1) hit3 = (tag3 == tag);
	CORE_UINT(1) hit4 = (tag4 == tag);

	CORE_UINT(1) hit = hit1 | hit2 | hit3 | hit4;
	CORE_UINT(128) value;

	if (hit1)
		value = val1.SLC(128, TAG_SIZE);
	if (hit2)
		value = val2.SLC(128, TAG_SIZE);
	if (hit3)
		value = val3.SLC(128, TAG_SIZE);
	if (hit4)
		value = val4.SLC(128, TAG_SIZE);

	if (hit){
		dataOut = value.SLC(32, 8*offset);
	}
	else{
	    miss = 5;
	    stall = 1;
	}
	}
	else{

        if (miss == 5)
		  CORE_UINT(128+TAG_SIZE) newVal = tag;
		//CORE_UINT(32) age1 = ages[4*offset+0];
		//CORE_UINT(32) age2 = ages[4*offset+1];
		//CORE_UINT(32) age3 = ages[4*offset+2];
		//CORE_UINT(32) age4 = ages[4*offset+3];

        if (miss >= 2)
		  newVal.SET_SLC((miss-2)*8+TAG_SIZE, externalMemory[(address>>2) + (miss-2)]);

        miss--;
		//if (age1<age2 & age1<age3 & age1<age4)
		if (miss == 1){
			data_set1[place] = newVal;

		      int newOffset = TAG_SIZE + 8*offset;
		      dataOut = newVal.SLC(32, newOffset);

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
}

