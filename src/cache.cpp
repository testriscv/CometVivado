#include "cache.h"

void cacheWrapper(ac_int<IWidth, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], unsigned int cim[Sets][Blocksize][Associativity],
                  ICacheRequest irequest, ICacheReply& ireply,
                  ac_int<DWidth, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], unsigned int cdm[Sets][Blocksize][Associativity],
                  DCacheRequest drequest, DCacheReply& dreply)
{
#ifdef __HLS__
    static ICacheControl ictrl;
    static DCacheControl dctrl;

    icache<0>(memictrl, imem, cim, irequest, ireply);
    dcache<0>(memdctrl, dmem, cdm, drequest, dreply);
#endif
}

bool find(ICacheControl& ictrl, ac_int<32, false> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    findiloop:for(int i = 0; i < Associativity; ++i)
    {
        if((ictrl.setctrl.tag[i] == getTag(address)) && ictrl.setctrl.valid[i])
        {
            found = true;
            valid = true;
            ictrl.currentway = i;
        }
    }

    return found && valid;
}

bool find(DCacheControl& dctrl, ac_int<32, false> address)
{
    bool found = false;
    bool valid = false;

    #pragma hls_unroll yes
    finddloop:for(int i = 0; i < Associativity; ++i)
    {
        if((dctrl.setctrl.tag[i] == getTag(address)) && dctrl.setctrl.valid[i])
        {
            found = true;
            valid = true;
            dctrl.currentway = i;
        }
    }

    return found && valid;
}

void select(ICacheControl& ictrl)
{
#if Associativity > 1
  #if Policy == RP_FIFO
    ictrl.currentway = ictrl.setctrl.policy;
  #elif Policy == RP_LRU
    #if Associativity == 2
        ictrl.currentway = !ictrl.setctrl.policy;
    #elif Associativity == 4
        if(ictrl.setctrl.policy.slc<3>(3) == 0)
        {
            ictrl.currentway = 3;
        }
        else if(ictrl.setctrl.policy.slc<2>(1) == 0)
        {
            ictrl.currentway = 2;
        }
        else if(ictrl.setctrl.policy.slc<1>(0) == 0)
        {
            ictrl.currentway = 1;
        }
        else
        {
            ictrl.currentway = 0;
        }
    #else
        #error "RP_LRU with N >= 8 ways associativity is not implemented"
    #endif
  #elif Policy == RP_RANDOM
    ictrl.currentway = ictrl.setctrl.policy.slc<ac::log2_ceil<Associativity>::val>(0);     // ictrl.setctrl.policy & (Associativity - 1)
  #else   // None
    ictrl.currentway = 0;
  #endif
#endif
}

void select(DCacheControl& dctrl)
{
#if Associativity > 1
  #if Policy == RP_FIFO
    dctrl.currentway = dctrl.setctrl.policy;
  #elif Policy == RP_LRU
    #if Associativity == 2
        dctrl.currentway = !dctrl.setctrl.policy;
    #elif Associativity == 4
        if(dctrl.setctrl.policy.slc<3>(3) == 0)
        {
            dctrl.currentway = 3;
        }
        else if(dctrl.setctrl.policy.slc<2>(1) == 0)
        {
            dctrl.currentway = 2;
        }
        else if(dctrl.setctrl.policy.slc<1>(0) == 0)
        {
            dctrl.currentway = 1;
        }
        else
        {
            dctrl.currentway = 0;
        }
    #else
        #error "RP_LRU with N >= 8 ways associativity is not implemented"
    #endif
  #elif Policy == RP_RANDOM
    dctrl.currentway = dctrl.setctrl.policy.slc<ac::log2_ceil<Associativity>::val>(0);     // dctrl.setctrl.policy & (Associativity - 1)
  #else   // None
    dctrl.currentway = 0;
  #endif
#endif
}

void update_policy(ICacheControl& ictrl)
{
#if Associativity > 1
  #if Policy == RP_FIFO
    // no promotion
  #elif Policy == RP_LRU
    #if Associativity == 2
        ictrl.setctrl.policy = ictrl.currentway;
    #elif Associativity == 4
        switch (ictrl.currentway) {
        case 0:
            ictrl.setctrl.policy.set_slc(0, (ac_int<1, false>)0);
            ictrl.setctrl.policy.set_slc(1, (ac_int<1, false>)0);
            ictrl.setctrl.policy.set_slc(3, (ac_int<1, false>)0);
            break;
        case 1:
            ictrl.setctrl.policy.set_slc(0, (ac_int<1, false>)1);
            ictrl.setctrl.policy.set_slc(2, (ac_int<1, false>)0);
            ictrl.setctrl.policy.set_slc(4, (ac_int<1, false>)0);
            break;
        case 2:
            ictrl.setctrl.policy.set_slc(1, (ac_int<1, false>)1);
            ictrl.setctrl.policy.set_slc(2, (ac_int<1, false>)1);
            ictrl.setctrl.policy.set_slc(5, (ac_int<1, false>)0);
            break;
        case 3:
            ictrl.setctrl.policy.set_slc(3, (ac_int<1, false>)1);
            ictrl.setctrl.policy.set_slc(4, (ac_int<1, false>)1);
            ictrl.setctrl.policy.set_slc(5, (ac_int<1, false>)1);
            break;
        default:
            break;
        }
    #else
        #error "RP_LRU with N >= 8 ways associativity is not implemented"
    #endif
  #elif Policy == RP_RANDOM
    // no promotion
  #else   // None

  #endif
#endif
}

void update_policy(DCacheControl& dctrl)
{
#if Associativity > 1
  #if Policy == RP_FIFO
    // no promotion
  #elif Policy == RP_LRU
    #if Associativity == 2
        dctrl.setctrl.policy = dctrl.currentway;
    #elif Associativity == 4
        switch (dctrl.currentway) {
        case 0:
            dctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)0);
            dctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)0);
            dctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)0);
            break;
        case 1:
            dctrl.setctrl.policy.set_slc(0, (ac_int<1, false>)1);
            dctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)0);
            dctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)0);
            break;
        case 2:
            dctrl.setctrl.policy.set_slc(1, (ac_int<1, false>)1);
            dctrl.setctrl.policy.set_slc(2, (ac_int<1, false>)1);
            dctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)0);
            break;
        case 3:
            dctrl.setctrl.policy.set_slc(3, (ac_int<1, false>)1);
            dctrl.setctrl.policy.set_slc(4, (ac_int<1, false>)1);
            dctrl.setctrl.policy.set_slc(5, (ac_int<1, false>)1);
            break;
        default:
            break;
        }
    #else
        #error "RP_LRU with N >= 8 ways associativity is not implemented"
    #endif
  #elif Policy == RP_RANDOM
    // no promotion
  #else   // None

  #endif
#endif
}

void insert_policy(ICacheControl& ictrl)
{
#if Associativity > 1
  #if Policy == RP_FIFO
    ictrl.setctrl.policy++;
  #elif Policy == RP_LRU    // insertion & promotion are same
    update_policy(ictrl);
  #elif Policy == RP_RANDOM
    ictrl.setctrl.policy = (ictrl.setctrl.policy.slc<1>(31) ^ ictrl.setctrl.policy.slc<1>(21) ^ ictrl.setctrl.policy.slc<1>(1) ^ ictrl.setctrl.policy.slc<1>(0)) | (ictrl.setctrl.policy << 1);
  #endif
#endif
}

void insert_policy(DCacheControl& dctrl)
{
#if Associativity > 1
  #if Policy == RP_FIFO
    dctrl.setctrl.policy++;
  #elif Policy == RP_LRU
    update_policy(dctrl);
  #elif Policy == RP_RANDOM
    dctrl.setctrl.policy = (dctrl.setctrl.policy.slc<1>(31) ^ dctrl.setctrl.policy.slc<1>(21) ^ dctrl.setctrl.policy.slc<1>(1) ^ dctrl.setctrl.policy.slc<1>(0)) | (dctrl.setctrl.policy << 1);
  #endif
#endif
}
