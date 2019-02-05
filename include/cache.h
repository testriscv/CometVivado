#ifndef CACHE_H
#define CACHE_H

#include "memory.h"

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
  #if Policy == RP_NONE
    #define Associativity           1
  #else
    #define Associativity           4
  #endif
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

#if Policy == RP_FIFO
#define IPolicyWidth         (ac::log2_ceil<Associativity>::val)
#define DPolicyWidth         (ac::log2_ceil<Associativity>::val)
#elif Policy == RP_LRU
#define IPolicyWidth         (Associativity * (Associativity-1) / 2)
#define DPolicyWidth         (Associativity * (Associativity-1) / 2)
#else
#define IPolicyWidth         0      // ok as long as it's protected by #if when used in slc method
#define DPolicyWidth         0
#endif
#define ICacheControlWidth   (Associativity*(32-tagshift+1)+IPolicyWidth)   // tag + valid bit
#define DCacheControlWidth   (Associativity*(32-tagshift+2)+DPolicyWidth)   // tag + valid + dirty bit
#define ibourrage            ((1 << ac::log2_ceil<ICacheControlWidth>::val) - ICacheControlWidth)
#define dbourrage            ((1 << ac::log2_ceil<DCacheControlWidth>::val) - DCacheControlWidth)

#ifndef __HLS__
#define IWidth               (1 << ac::log2_ceil<ICacheControlWidth>::val)
#define DWidth               (1 << ac::log2_ceil<DCacheControlWidth>::val)
#else       // 128 instead of 1 << ac::log2_ceil<XCacheControlWidth>::val because st mem is 128 bits
#define IWidth               128
#define DWidth               128
#endif

struct ISetControl
{
    ISetControl()
    : bourrage(0)
  #if Associativity > 1
      , policy
    #if Policy == RP_RANDOM
        (0xF2D4B698)
    #else
        (0)
    #endif
  #endif
    {
        #pragma hls_unroll yes
        for(int i(0); i < Associativity; ++i)
        {
            data[i] = 0;
            tag[i] = 0;
            valid[i] = false;
        }
    }

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
    ac_int<32, false> policy;
  #else   // None
    #error "Cannot have Associativity > 1 and no policy"
  #endif
#endif
};

struct ICacheControl
{
    ICacheControl()
    : state(ICacheControl::Idle), workAddress(0), ctrlLoaded(false), i(0), valuetowrite(0),
      currentset(0), currentway(0), memcnt(0), setctrl()
    {}

    enum IState
    {
        Idle            ,
        StoreControl    ,
        Fetch           ,
        IStates
    } state : ac::log2_ceil<IStates+1>::val;
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

    // counter to simulate the slow main memory
    ac_int<ac::log2_ceil<AC_MAX(MEMORY_READ_LATENCY, MEMORY_WRITE_LATENCY)>::val, false> memcnt;

    ISetControl setctrl;
};

struct ICacheRequest
{
    ICacheRequest()
    : address(0)
    {}

    ac_int<32, false> address;
};

struct ICacheReply
{
    ICacheReply()
    : cachepc(0), instruction(0), insvalid(false)
    {}

    ac_int<32, false> cachepc;
    int instruction;
    bool insvalid;
};

struct DSetControl
{
    DSetControl()
    : bourrage(0)
  #if Associativity > 1
    , policy
    #if Policy == RP_RANDOM
      (0xF2D4B698)
    #else
      (0)
    #endif
  #endif
    {
        #pragma hls_unroll yes
        for(int i(0); i < Associativity; ++i)
        {
            data[i] = 0;
            tag[i] = 0;
            dirty[i] = false;
            valid[i] = false;
        }
    }

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
    ac_int<32, false> policy;
  #else   // None
    #error "Cannot have Associativity > 1 and no policy"
  #endif
#endif
};

struct DCacheControl
{
    DCacheControl()
    : state(DCacheControl::Idle), workAddress(0), i(0), valuetowrite(0),
      currentset(0), currentway(0), memcnt(0), setctrl()
    {}

    enum DState
    {
        Idle            ,
        StoreControl    ,
        StoreData       ,
        FirstWriteBack  ,
        WriteBack       ,
        Fetch           ,
        DStates
    } state : ac::log2_ceil<DStates+1>::val;
    ac_int<32, false> workAddress;
    ac_int<ac::log2_ceil<Blocksize>::val, false> i;
    ac_int<32, false> valuetowrite;
    ac_int<ac::log2_ceil<Sets>::val, false> currentset;
#if Associativity == 1
    ac_int<1, false> currentway;
#else
    ac_int<ac::log2_ceil<Associativity>::val, false> currentway;
#endif

    // counter to simulate the slow main memory
    ac_int<ac::log2_ceil<AC_MAX(MEMORY_READ_LATENCY, MEMORY_WRITE_LATENCY)>::val, false> memcnt;

    DSetControl setctrl;
};

struct DCacheRequest
{
    DCacheRequest()
    : address(0), datasize(0), signenable(false), dcacheenable(false),
      writeenable(false), writevalue(0)
    {}

    ac_int<32, false> address;
    ac_int<2, false> datasize;
    bool signenable;
    bool dcacheenable;
    bool writeenable;
    int writevalue;
};

struct DCacheReply
{
    DCacheReply()
    : readvalue(0), datavalid(false)
    {}

    int readvalue;
    bool datavalid;
};

class Simulator;

void icache(ac_int<IWidth, false> memctrl[Sets], unsigned int imem[DRAM_SIZE],  // control & memory
            unsigned int data[Sets][Blocksize][Associativity],                  // cachedata
            ICacheRequest irequest, ICacheReply& ireply                         // from & to cpu
        #ifndef __HLS__
            , Simulator* sim
        #endif
            );

void dcache(ac_int<DWidth, false> memctrl[Sets], unsigned int dmem[DRAM_SIZE],  // control & memory
            unsigned int data[Sets][Blocksize][Associativity],                  // cachedata
            DCacheRequest drequest, DCacheReply& dreply                         // from & to cpu
        #ifndef __HLS__
            , Simulator* sim
        #endif
            );

// wrapper to synthesize caches only
void cacheWrapper(ac_int<IWidth, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], unsigned int cim[Sets][Blocksize][Associativity],
                  ICacheRequest irequest, ICacheReply& ireply,
                  ac_int<DWidth, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], unsigned int cdm[Sets][Blocksize][Associativity],
                  DCacheRequest drequest, DCacheReply& dreply);


#endif // CACHE_H
