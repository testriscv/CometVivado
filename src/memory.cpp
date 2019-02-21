/*
 * memory.cpp
 *
 *  Created on: 25 janv. 2019
 *      Author: simon
 */

#include <memory.h>
#include <cacheMemory.h>
#include <memoryInterface.h>
#include <ac_int.h>

#define OFFSET_SIZE 4
#define TAG_SIZE 32-3-4
#define ASSOCIATIVITY 4
#define SET_SIZE 256


