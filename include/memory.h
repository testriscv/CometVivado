#ifndef __MEMORY_H__
#define __MEMORY_H__

#define STRINGIFY(a) #a

#ifdef __SYNTHESIS__
#define __HLS__
#elif CCS_DUT_RTL || CCS_DUT_SYSC // modelsim defines
#define __HLS__
#endif

#ifdef __VIVADO__
#include <ap_int.h>
#define CORE_UINT(param) ap_uint<param>
#define CORE_INT(param) ap_int<param>
#define SLC(size, low) range(size + low - 1, low)
#define SET_SLC(low, value) range(low + value.length() - 1, low) = value

#define HLS_PIPELINE(param) _Pragma(STRINGIFY(HLS PIPELINE II = param))
#define HLS_UNROLL(param) _Pragma(STRINGIFY(HLS UNROLL factor = param))
#define HLS_TOP(param) _Pragma(STRINGIFY(HLS top name = param))
#define HLS_DESIGN
#elif __CATAPULT__
#include <ac_int.h>
#define CORE_UINT(param) ac_int<param, false>
#define CORE_INT(param) ac_int<param, true>
#define SLC(size, low) slc<size>(low)
#define SET_SLC(low, value) set_slc(low, value)

#define HLS_PIPELINE(param) _Pragma(STRINGIFY(hls_pipeline_init_interval param))
#define HLS_UNROLL(param) _Pragma(STRINGIFY(hls_unroll param))
#define HLS_TOP(param) _Pragma(STRINGIFY(hls_design top))
#define HLS_DESIGN _Pragma(STRINGIFY(hls_design))
#endif

#ifndef __HLS__
#include <cstdio>
#include <stdint.h>
#define simul(...) __VA_ARGS__

#else
#define simul(...)

#endif

#define INSTR_MEMORY_WIDTH 32
#define DRAM_WIDTH 32
#ifndef __HLS__
#define DRAM_SIZE ((size_t)1 << 26)
#else
#define DRAM_SIZE 8192
#endif

#define STACK_INIT (DRAM_SIZE - 0x1000)

#ifndef MEMORY_READ_LATENCY
#define MEMORY_READ_LATENCY 5
#endif

#ifndef MEMORY_WRITE_LATENCY
#define MEMORY_WRITE_LATENCY 5
#endif

void formatread(ac_int<32, false> address, ac_int<2, false> datasize, bool sign, ac_int<32, false>& read);
void formatwrite(ac_int<32, false> address, ac_int<2, false> datasize, ac_int<32, false>& mem, ac_int<32, false> write);

#endif /* __MEMORY_H__ */
