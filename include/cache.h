#ifndef CACHE_H
#define CACHE_H

#include "portability.h"

#define RP_NONE                     0
#define RP_FIFO                     1
#define RP_LRU                      2
#define RP_RANDOM                   3

#ifndef Policy
#define Policy                      RP_LRU
#endif

#ifndef Size
#define Size                        1024    // bytes
#endif

#ifndef Blocksize
#define Blocksize                   (32/sizeof(int))  // 32 bytes
#endif

#ifndef Associativity
#define Associativity               4
#else
#if Associativity == 0
#undef Associativity
#pragma message "Associativity was 0, set to 1"
#define Associativity 1
#endif
#endif

#define Sets                        (Size/(Blocksize*sizeof(int))/Associativity)

#define blockshift                  2
#define setshift                    (ac::log2_ceil<Blocksize>::val + 2)
#define tagshift                    (ac::log2_ceil<Sets>::val + setshift)

#define offmask                     (0x3)
#define blockmask                   ((Blocksize - 1) << 2)
#define setmask                     ((Sets - 1) << setshift)
#define tagmask                     (~(blockmask + setmask + offmask))

#if 1
#define getTag(address)             (address.slc<32-tagshift>(tagshift))
#define getSet(address)             (address.slc<ac::log2_ceil<Sets>::val>(setshift))
#define getOffset(address)          (address.slc<ac::log2_ceil<Blocksize>::val>(2))

#define setTag(address, tag)        (address.set_slc(tagshift, (ac_int<32-tagshift, false>)tag))
#define setSet(address, set)        (address.set_slc(setshift, (ac_int<ac::log2_ceil<Sets>::val, false>)set))
#define setOffset(address, offset)  (address.set_slc(blockshift, (ac_int<ac::log2_ceil<Blocksize>::val, false>)offset))
#else
#define getTag(address)             (address  >> tagshift)
#define getSet(address)             ((address & setmask) >> setshift)
#define getOffset(address)          ((address & blockmask) >> 2)

#define setTag(address, tag)        (address = ((tag) << tagshift) | (address & ~tagmask))
#define setSet(address, set)        (address = ((set) << setshift) | (address & ~setmask))
#define setOffset(address, offset)  (address = ((offset) << blockshift) | (address & ~blockmask))
#endif

enum {
    MemtoCache = 0,
    CachetoMem = 1
};

namespace IState {
enum IState {
    Idle            ,
    StoreControl    ,
    Fetch           ,
    IStates
};
}

namespace DState {
enum DState {
    Idle            ,
    StoreControl    ,
    StoreData       ,
    FirstWriteBack  ,
    WriteBack       ,
    Fetch           ,
    DStates
};
}

#if Policy == RP_FIFO
#define IPolicyWidth         (ac::log2_ceil<Associativity>::val)
#define DPolicyWidth         (ac::log2_ceil<Associativity>::val)
#elif Policy == RP_LRU
#define IPolicyWidth         (Associativity * (Associativity-1) / 2)
#define DPolicyWidth         (Associativity * (Associativity-1) / 2)
#endif
#define ICacheControlWidth   (Associativity*(32-tagshift+1)+IPolicyWidth)   // tag + valid bit
#define DCacheControlWidth   (Associativity*(32-tagshift+2)+DPolicyWidth)   // tag + valid + dirty bit
#define ibourrage            ((1 << ac::log2_ceil<ICacheControlWidth>::val) - ICacheControlWidth)
#define dbourrage            ((1 << ac::log2_ceil<DCacheControlWidth>::val) - DCacheControlWidth)

struct ISetControl
{
    unsigned int data[Associativity];
    ac_int<32-tagshift, false> tag[Associativity];
    bool valid[Associativity];
    ac_int<ibourrage, false> bourrage;

#if Associativity > 1
  #if Policy == RP_FIFO
    ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #elif Policy == RP_LRU
    ac_int<Associativity * (Associativity-1) / 2, false> policy;
  #elif Policy == RP_RANDOM
    //ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #else   // None
    //ac_int<1, false> policy;
  #endif
#endif
};

struct ICacheControl
{
    IState::IState state;
    ac_int<32, false> workAddress;
    bool ctrlLoaded;
    ac_int<ac::log2_ceil<Blocksize>::val, false> i;
    ac_int<32, false> valuetowrite;
    ac_int<ac::log2_ceil<Sets>::val, false> currentset;
#if Associativity == 1
    ac_int<1, false> currentway;
#else
    ac_int<ac::log2_ceil<Associativity>::val, false> currentway;
#endif

    ISetControl setctrl;
};

struct DSetControl
{
    unsigned int data[Associativity];
    ac_int<32-tagshift, false> tag[Associativity];
    bool dirty[Associativity];
    bool valid[Associativity];
    ac_int<dbourrage, false> bourrage;

#if Associativity > 1
  #if Policy == RP_FIFO
    ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #elif Policy == RP_LRU
    ac_int<Associativity * (Associativity-1) / 2, false> policy;
  #elif Policy == RP_RANDOM
    ac_int<ac::log2_ceil<Associativity>::val, false> policy;
  #else   // None
    //ac_int<1, false> policy;
  #endif
#endif
};

struct DCacheControl
{
    DState::DState state;
    ac_int<32, false> workAddress;
    ac_int<ac::log2_ceil<Blocksize>::val, false> i;
    ac_int<32, false> valuetowrite;
    ac_int<ac::log2_ceil<Sets>::val, false> currentset;
#if Associativity == 1
    ac_int<1, false> currentway;
#else
    ac_int<ac::log2_ceil<Associativity>::val, false> currentway;
#endif

    DSetControl setctrl;
};

void icache(ICacheControl& ctrl, ac_int<128, false> memctrl[Sets],                              // control
            unsigned int imem[DRAM_SIZE], unsigned int data[Sets][Blocksize][Associativity],    // memory and cachedata
            ac_int<32, false> iaddress,                                                         // from cpu
            ac_int<32, false> &cachepc, int& instruction, bool& insvalid                        // to cpu
#ifndef __HLS__
           , ac_int<64, false>& cycles
#endif
           );
void dcache(DCacheControl& ctrl, ac_int<128, false> memctrl[Sets],                              // control
            unsigned int dmem[DRAM_SIZE], unsigned int data[Sets][Blocksize][Associativity],    // memory and cachedata
            ac_int<32, false> address, ac_int<2, false> datasize, bool signenable, bool dcacheenable, bool writeenable, int writevalue,    // from cpu
            int& read, bool& datavalid                                                          // to cpu
#ifndef __HLS__
           , ac_int<64, false>& cycles
#endif
           );

// wrapper to synthesize caches only
void cacheWrapper(ac_int<128, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], unsigned int idata[Sets][Blocksize][Associativity],
                  ac_int<128, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], unsigned int ddata[Sets][Blocksize][Associativity]);

#endif // CACHE_H
