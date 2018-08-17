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
    ac_int<32, false> policy;
  #else   // None
    #error "Cannot have Associativity > 1 and no policy"
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

    // counter to simulate the slow main memory
    ac_int<ac::log2_ceil<AC_MAX(MEMORY_READ_LATENCY, MEMORY_WRITE_LATENCY)>::val, false> memcnt;

    ISetControl setctrl;
};

struct ICacheRequest
{
    ac_int<32, false> address;
};

struct ICacheReply
{
    ac_int<32, false> cachepc;
    int instruction;
    bool insvalid;
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
    ac_int<32, false> policy;
  #else   // None
    #error "Cannot have Associativity > 1 and no policy"
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

    // counter to simulate the slow main memory
    ac_int<ac::log2_ceil<AC_MAX(MEMORY_READ_LATENCY, MEMORY_WRITE_LATENCY)>::val, false> memcnt;

    DSetControl setctrl;
};

struct DCacheRequest
{
    ac_int<32, false> address;
    ac_int<2, false> datasize;
    bool signenable;
    bool dcacheenable;
    bool writeenable;
    int writevalue;
};

struct DCacheReply
{
    int readvalue;
    bool datavalid;
};
/*
template<unsigned int hartid>
void icache(ac_int<IWidth, false> memctrl[Sets], unsigned int imem[DRAM_SIZE],  // control & memory
            unsigned int data[Sets][Blocksize][Associativity],                  // cachedata
            ICacheRequest irequest, ICacheReply& ireply);                       // from & to cpu

template<unsigned int hartid>
void dcache(ac_int<DWidth, false> memctrl[Sets], unsigned int dmem[DRAM_SIZE],  // control & memory
            unsigned int data[Sets][Blocksize][Associativity],                  // cachedata
            DCacheRequest drequest, DCacheReply& dreply);                       // from & to cpu

// wrapper to synthesize caches only
void cacheWrapper(ac_int<IWidth, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], unsigned int cim[Sets][Blocksize][Associativity],
                  ICacheRequest irequest, ICacheReply& ireply,
                  ac_int<DWidth, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], unsigned int cdm[Sets][Blocksize][Associativity],
                  DCacheRequest drequest, DCacheReply& dreply);*/

void cacheWrapper(ac_int<IWidth, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], unsigned int cim[Sets][Blocksize][Associativity],
                  ICacheRequest irequest, ICacheReply& ireply,
                  ac_int<DWidth, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], unsigned int cdm[Sets][Blocksize][Associativity],
                  DCacheRequest drequest, DCacheReply& dreply);

bool find(ICacheControl& ictrl, ac_int<32, false> address);
bool find(DCacheControl& dctrl, ac_int<32, false> address);

void select(ICacheControl& ictrl);
void select(DCacheControl& dctrl);

void update_policy(ICacheControl& ictrl);
void update_policy(DCacheControl& dctrl);

void insert_policy(ICacheControl& ictrl);
void insert_policy(DCacheControl& dctrl);

template<unsigned int hartid>
void icache(ac_int<IWidth, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], // control & memory
            unsigned int data[Sets][Blocksize][Associativity],                  // cachedata
            ICacheRequest irequest, ICacheReply& ireply)                        // from & to cpu
{
    static ICacheControl ictrl;

    if(ictrl.state != IState::Fetch && ictrl.currentset != getSet(irequest.address))  // different way but same set keeps same control, except for data......
    {
        ictrl.state = IState::StoreControl;
        gdebug("irequest.address %06x storecontrol\n", (ictrl.setctrl.tag[ictrl.currentway].to_int() << tagshift) | (ictrl.currentset.to_int() << setshift));
    }

    switch(ictrl.state)
    {
    case IState::Idle:
        ictrl.currentset = getSet(irequest.address);
        ictrl.i = getOffset(irequest.address);

        if(!ictrl.ctrlLoaded)
        {
            ac_int<IWidth, false> setctrl = memictrl[ictrl.currentset];
            ictrl.setctrl.bourrage = setctrl.slc<ibourrage>(ICacheControlWidth);
            #pragma hls_unroll yes
            loadiset:for(int i = 0; i < Associativity; ++i)
            {
                ictrl.setctrl.tag[i] = setctrl.slc<32-tagshift>(i*(32-tagshift));
                ictrl.setctrl.valid[i] = setctrl.slc<1>(Associativity*(32-tagshift) + i);
            }
        #if Associativity > 1 && (Policy == RP_FIFO || Policy == RP_LRU)
            ictrl.setctrl.policy = setctrl.slc<IPolicyWidth>(Associativity*(32-tagshift+1));
        #endif
        }

        #pragma hls_unroll yes
        loadidata:for(int i = 0; i < Associativity; ++i)    // force reload because offset may have changed
        {
            ictrl.setctrl.data[i] = data[ictrl.currentset][ictrl.i][i];
        }

        ictrl.workAddress = irequest.address;
        ictrl.ctrlLoaded = true;

        if(find(ictrl, irequest.address))
        {
            ireply.instruction = ictrl.setctrl.data[ictrl.currentway];

            ictrl.state = IState::Idle;

            update_policy(ictrl);

            ireply.insvalid = true;
            ireply.cachepc = irequest.address;
        }
        else    // not found or invalid
        {
            select(ictrl);
            coredebug("cim  @%06x   not found or invalid   ", irequest.address.to_int());
            ictrl.setctrl.tag[ictrl.currentway] = getTag(irequest.address);

            ictrl.state = IState::Fetch;
            ictrl.setctrl.valid[ictrl.currentway] = false;
            ictrl.i = getOffset(irequest.address);
            ac_int<32, false> wordad = 0;
            wordad.set_slc(0, irequest.address.slc<30>(2));
            wordad.set_slc(30, (ac_int<2, false>)0);
            coredebug("starting fetching to %d %d from %06x to %06x (%06x to %06x)\n", ictrl.currentset.to_int(), ictrl.currentway.to_int(), (wordad.to_int() << 2)&(tagmask+setmask),
                  (((int)(wordad.to_int()+Blocksize) << 2)&(tagmask+setmask))-1, (irequest.address >> 2).to_int() & (~(blockmask >> 2)), (((irequest.address >> 2).to_int() + Blocksize) & (~(blockmask >> 2)))-1);
            ictrl.valuetowrite = imem[wordad];
            ictrl.memcnt = 1;
            // critical word first
            ireply.instruction = ictrl.valuetowrite;

            insert_policy(ictrl);

            ireply.insvalid = false;
        }
        break;
    case IState::StoreControl:
        #pragma hls_unroll yes
        if(ictrl.ctrlLoaded)        // this prevent storing false control when we jump to another jump ireply.instruction
        {
            gdebug("StoreControl for %d %d  %06x to %06x\n", ictrl.currentset.to_int(), ictrl.currentway.to_int(),
                        (ictrl.setctrl.tag[ictrl.currentway].to_int() << tagshift) | (ictrl.currentset.to_int() << setshift),
                        ((ictrl.setctrl.tag[ictrl.currentway].to_int() << tagshift) | (ictrl.currentset.to_int() << setshift))+Blocksize*4-1);

            ac_int<IWidth, false> setctrl = 0;
            setctrl.set_slc(ICacheControlWidth, ictrl.setctrl.bourrage);
            #pragma hls_unroll yes
            storeicontrol:for(int i = 0; i < Associativity; ++i)
            {
                setctrl.set_slc(i*(32-tagshift), ictrl.setctrl.tag[i]);
                setctrl.set_slc(Associativity*(32-tagshift) + i, (ac_int<1, false>)ictrl.setctrl.valid[i]);
                gdebug("tag : %6x      valid : %s\n", (ictrl.setctrl.tag[i].to_int() << tagshift) | (ictrl.currentset.to_int() << setshift), ictrl.setctrl.valid[i]?"true":"false");
            }
        #if Associativity > 1 && (Policy == RP_FIFO || Policy == RP_LRU)
            setctrl.set_slc(Associativity*(32-tagshift+1), ictrl.setctrl.policy);
        #endif
            memictrl[ictrl.currentset] = setctrl;
        }
        else
        {
            gdebug("StoreControl but control not loaded\n");
        }

        ictrl.state = IState::Idle;
        ictrl.currentset = getSet(irequest.address);  //use workaddress?
        ictrl.workAddress = irequest.address;
        ictrl.ctrlLoaded = false;
        ireply.insvalid = false;
        break;
    case IState::Fetch:
        if(ictrl.memcnt == MEMORY_READ_LATENCY)
        {
            data[ictrl.currentset][ictrl.i][ictrl.currentway] = ictrl.valuetowrite;
            ireply.instruction = ictrl.valuetowrite;
            ireply.cachepc = ictrl.workAddress;
            ireply.cachepc.set_slc(2, ictrl.i);
            ireply.insvalid = true;

            if(++ictrl.i != getOffset(ictrl.workAddress))
            {
                ac_int<32, false> bytead = 0;
                setTag(bytead, ictrl.setctrl.tag[ictrl.currentway]);
                setSet(bytead, ictrl.currentset);
                setOffset(bytead, ictrl.i);

                ictrl.valuetowrite = imem[bytead >> 2];
            }
            else
            {
                ictrl.state = IState::Idle;
                ictrl.setctrl.valid[ictrl.currentway] = true;
                ictrl.ctrlLoaded = true;
                //ictrl.currentset = getSet(irequest.address);  //use workaddress?
                //ictrl.workAddress = irequest.address;
                ireply.insvalid = false;
            }
            ictrl.memcnt = 0;
        }
        else
        {
            ictrl.memcnt++;
            ireply.insvalid = false;
        }

        break;
    default:
        ireply.insvalid = false;
        ictrl.state = IState::Idle;
        ictrl.ctrlLoaded = false;
        break;
    }

    simul(if(ireply.insvalid)
    {
        coredebug("i    @%06x   %08x    %d %d\n", ireply.cachepc.to_int(), ireply.instruction, ictrl.currentset.to_int(), ictrl.currentway.to_int());
    })

}

template<unsigned int hartid>
void dcache(ac_int<DWidth, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], // control & memory
             unsigned int data[Sets][Blocksize][Associativity],                 // cachedata
            DCacheRequest drequest, DCacheReply& dreply)                        // from & to cpu                                          // to cpu
{
    /*if(dcacheenable && datavalid)   // we can avoid storing control if we hit same set multiple times in a row
    {
        if(dctrl.currentset != getSet(address))
        {
            dctrl.state == DState::StoreControl;
        }
        else
        {
            dctrl.state == DState::Idle;
        }
    }
    else if(!dcacheenable && datavalid)
    {
        dctrl.state == DState::StoreControl;
    }*/

    static DCacheControl dctrl;

    ac_int<32, false> address = drequest.address;
    ac_int<2, false> datasize = drequest.datasize;
    bool signenable = drequest.signenable;
    bool dcacheenable = drequest.dcacheenable;
    bool writeenable = drequest.writeenable;
    int writevalue = drequest.writevalue;

    int read = 0;
    bool datavalid = false;

    switch(dctrl.state)
    {
    case DState::Idle:
        if(dcacheenable)
        {
            dctrl.currentset = getSet(address);
            dctrl.i = getOffset(address);

            ac_int<DWidth, false> setctrl = memdctrl[dctrl.currentset];
            dctrl.setctrl.bourrage = setctrl.slc<dbourrage>(DCacheControlWidth);
            #pragma hls_unroll yes
            loaddset:for(int i = 0; i < Associativity; ++i)
            {
                dctrl.setctrl.data[i] = data[dctrl.currentset][dctrl.i][i];

                dctrl.setctrl.tag[i] = setctrl.slc<32-tagshift>(i*(32-tagshift));
                dctrl.setctrl.valid[i] = setctrl.slc<1>(Associativity*(32-tagshift) + i);
                dctrl.setctrl.dirty[i] = setctrl.slc<1>(Associativity*(32-tagshift+1) + i);
            }
        #if Associativity > 1 && (Policy == RP_FIFO || Policy == RP_LRU)
            dctrl.setctrl.policy = setctrl.slc<DPolicyWidth>(Associativity*(32-tagshift+2));
        #endif

            if(find(dctrl, address))
            {
                if(writeenable)
                {
                    dctrl.valuetowrite = dctrl.setctrl.data[dctrl.currentway];
                    formatwrite(address, datasize, dctrl.valuetowrite, writevalue);
                    dctrl.workAddress = address;
                    dctrl.setctrl.dirty[dctrl.currentway] = true;

                    dctrl.state = DState::StoreData;
                }
                else
                {
                    ac_int<32, false> r = dctrl.setctrl.data[dctrl.currentway];
                    formatread(address, datasize, signenable, r);

                    read = r;

                    dctrl.state = DState::StoreControl;
                }
                update_policy(dctrl);
                datavalid = true;
            }
            else    // not found or invalid
            {
                select(dctrl);
                gdebug("cdm  @%06x   not found or invalid   ", address.to_int());
                if(dctrl.setctrl.dirty[dctrl.currentway] && dctrl.setctrl.valid[dctrl.currentway])
                {
                    dctrl.state = DState::FirstWriteBack;
                    dctrl.i = 0;
                    dctrl.workAddress = 0;
                    setTag(dctrl.workAddress, dctrl.setctrl.tag[dctrl.currentway]);
                    setSet(dctrl.workAddress, dctrl.currentset);
                    //dctrl.valuetowrite = dctrl.setctrl.data[dctrl.currentway];    // only if same offset than requested address
                    datavalid = false;
                    coredebug("starting writeback from %d %d from %06x to %06x\n", dctrl.currentset.to_int(), dctrl.currentway.to_int(), dctrl.workAddress.to_int(), dctrl.workAddress.to_int() + 4*Blocksize-1);
                }
                else
                {
                    dctrl.setctrl.tag[dctrl.currentway] = getTag(address);
                    dctrl.workAddress = address;
                    dctrl.state = DState::Fetch;
                    dctrl.setctrl.valid[dctrl.currentway] = false;
                    dctrl.i = getOffset(address);
                    ac_int<32, false> wordad = 0;
                    wordad.set_slc(0, address.slc<30>(2));
                    wordad.set_slc(30, (ac_int<2, false>)0);
                    coredebug("starting fetching to %d %d for %s from %06x to %06x (%06x to %06x)\n", dctrl.currentset.to_int(), dctrl.currentway.to_int(), writeenable?"W":"R", (wordad.to_int() << 2)&(tagmask+setmask),
                          (((int)(wordad.to_int()+Blocksize) << 2)&(tagmask+setmask))-1, (address >> 2).to_int() & (~(blockmask >> 2)), (((address >> 2).to_int() + Blocksize) & (~(blockmask >> 2)))-1 );
                    dctrl.valuetowrite = dmem[wordad];
                    dctrl.memcnt = 1;
                    // critical word first
                    if(writeenable)
                    {
                        formatwrite(address, datasize, dctrl.valuetowrite, writevalue);
                        dctrl.setctrl.dirty[dctrl.currentway] = true;
                    }
                    else
                    {
                        ac_int<32, false> r = dctrl.valuetowrite;
                        dctrl.setctrl.dirty[dctrl.currentway] = false;
                        formatread(address, datasize, signenable, r);
                        read = r;
                    }

                    datavalid = true;
                    insert_policy(dctrl);
                }
            }
        }
        else
            datavalid = false;
        break;
    case DState::StoreControl:
    {
        ac_int<DWidth, false> setctrl = 0;
        setctrl.set_slc(DCacheControlWidth, dctrl.setctrl.bourrage);
        #pragma hls_unroll yes
        storedcontrol:for(int i = 0; i < Associativity; ++i)
        {
            setctrl.set_slc(i*(32-tagshift), dctrl.setctrl.tag[i]);
            setctrl.set_slc(Associativity*(32-tagshift) + i, (ac_int<1, false>)dctrl.setctrl.valid[i]);
            setctrl.set_slc(Associativity*(32-tagshift+1) + i, (ac_int<1, false>)dctrl.setctrl.dirty[i]);
        }
    #if Associativity > 1 && (Policy == RP_FIFO || Policy == RP_LRU)
        setctrl.set_slc(Associativity*(32-tagshift+2), dctrl.setctrl.policy);
    #endif

        memdctrl[dctrl.currentset] = setctrl;
        dctrl.state = DState::Idle;
        datavalid = false;
        break;
    }
    case DState::StoreData:
    {
        ac_int<DWidth, false> setctrl = 0;
        setctrl.set_slc(DCacheControlWidth, dctrl.setctrl.bourrage);
        #pragma hls_unroll yes
        storedata:for(int i = 0; i < Associativity; ++i)
        {
            setctrl.set_slc(i*(32-tagshift), dctrl.setctrl.tag[i]);
            setctrl.set_slc(Associativity*(32-tagshift) + i, (ac_int<1, false>)dctrl.setctrl.valid[i]);
            setctrl.set_slc(Associativity*(32-tagshift+1) + i, (ac_int<1, false>)dctrl.setctrl.dirty[i]);
        }
    #if Associativity > 1 && (Policy == RP_FIFO || Policy == RP_LRU)
        setctrl.set_slc(Associativity*(32-tagshift+2), dctrl.setctrl.policy);
    #endif
        memdctrl[dctrl.currentset] = setctrl;

        data[dctrl.currentset][getOffset(dctrl.workAddress)][dctrl.currentway] = dctrl.valuetowrite;

        dctrl.state = DState::Idle;
        datavalid = false;
        break;
    }
    case DState::FirstWriteBack:
    {   //bracket for scope and allow compilation
        dctrl.i = 0;
        dctrl.memcnt = 0;
        ac_int<32, false> bytead = 0;
        setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
        setSet(bytead, dctrl.currentset);
        setOffset(bytead, dctrl.i);

        dctrl.valuetowrite = data[dctrl.currentset][dctrl.i][dctrl.currentway];
        dctrl.state = DState::WriteBack;
        datavalid = false;
        break;
    }
    case DState::WriteBack:
        if(dctrl.memcnt == MEMORY_WRITE_LATENCY)
        {   //bracket for scope and allow compilation
            ac_int<32, false> bytead = 0;
            setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
            setSet(bytead, dctrl.currentset);
            setOffset(bytead, dctrl.i);
            dmem[bytead >> 2] = dctrl.valuetowrite;

            if(++dctrl.i)
                dctrl.valuetowrite = data[dctrl.currentset][dctrl.i][dctrl.currentway];
            else
            {
                dctrl.state = DState::StoreControl;
                dctrl.setctrl.dirty[dctrl.currentway] = false;
                //gdebug("end of writeback\n");
            }
            dctrl.memcnt = 0;
        }
        else
        {
            dctrl.memcnt++;
        }
        datavalid = false;
        break;
    case DState::Fetch:
        if(dctrl.memcnt == MEMORY_READ_LATENCY)
        {
            data[dctrl.currentset][dctrl.i][dctrl.currentway] = dctrl.valuetowrite;

            if(++dctrl.i != getOffset(dctrl.workAddress))
            {
                ac_int<32, false> bytead = 0;
                setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
                setSet(bytead, dctrl.currentset);
                setOffset(bytead, dctrl.i);

                dctrl.valuetowrite = dmem[bytead >> 2];
            }
            else
            {
                dctrl.state = DState::StoreControl;
                dctrl.setctrl.valid[dctrl.currentway] = true;
                update_policy(dctrl);
                //gdebug("end of fetch to %d %d\n", dctrl.currentset.to_int(), dctrl.currentway.to_int());
            }
            dctrl.memcnt = 0;
        }
        else
        {
            dctrl.memcnt++;
        }
        datavalid = false;
        break;
    default:
        datavalid = false;
        dctrl.state = DState::Idle;
        break;
    }

    dreply.readvalue = read;
    dreply.datavalid = datavalid;

    simul(if(datavalid)
    {
        if(writeenable)
            coredebug("dW%d  @%06x   %08x   %08x   %08x   %d %d\n", datasize.to_int(), address.to_int(), dctrl.state == DState::Fetch?dmem[address/4]:data[dctrl.currentset][dctrl.i][dctrl.currentway],
                                                                      writevalue, dctrl.valuetowrite.to_int(), dctrl.currentset.to_int(), dctrl.currentway.to_int());
        else
            coredebug("dR%d  @%06x   %08x   %08x   %5s   %d %d\n", datasize.to_int(), address.to_int(), dctrl.state == DState::Fetch?dmem[address/4]:data[dctrl.currentset][dctrl.i][dctrl.currentway],
                                                                    read, signenable?"true":"false", dctrl.currentset.to_int(), dctrl.currentway.to_int());
    })
}




#endif // CACHE_H
