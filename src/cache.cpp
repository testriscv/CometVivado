#include "cache.h"

#ifndef __HLS__
#include "simulator.h"
#endif

void cacheWrapper(ac_int<IWidth, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], unsigned int cim[Sets][Blocksize][Associativity],
                  ICacheRequest irequest, ICacheReply& ireply,
                  ac_int<DWidth, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], unsigned int cdm[Sets][Blocksize][Associativity],
                  DCacheRequest drequest, DCacheReply& dreply)
{
#ifdef __HLS__
    static ICacheControl ictrl;
    static DCacheControl dctrl;

    icache(memictrl, imem, cim, irequest, ireply);
    dcache(memdctrl, dmem, cdm, drequest, dreply);
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

void icache(ac_int<IWidth, false> memictrl[Sets], unsigned int imem[DRAM_SIZE], // control & memory
            unsigned int data[Sets][Blocksize][Associativity],                  // cachedata
            ICacheRequest irequest, ICacheReply& ireply                         // from & to cpu
        #ifndef __HLS__
            , Simulator* sim
        #endif
            )
{
    static ICacheControl ictrl;

    if(ictrl.state != ICacheControl::Fetch && ictrl.currentset != getSet(irequest.address))  // different way but same set keeps same control, except for data......
    {
        ictrl.state = ICacheControl::StoreControl;
    }

    switch(ictrl.state)
    {
    case ICacheControl::Idle:
        ictrl.currentset = getSet(irequest.address);
        ictrl.i = getOffset(irequest.address);

        if(!ictrl.ctrlLoaded)
        {
            ac_int<IWidth, false> setctrl = memictrl[ictrl.currentset];
            ////simul(sim->icachedata.ctrlmemread++;)
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
        ////simul(sim->icachedata.cachememread+=Associativity;)

        ictrl.workAddress = irequest.address;
        ictrl.ctrlLoaded = true;

        if(find(ictrl, irequest.address))
        {
            ////simul(sim->icachedata.hit++;)

            ireply.instruction = ictrl.setctrl.data[ictrl.currentway];

            ictrl.state = ICacheControl::Idle;

            update_policy(ictrl);

            ireply.insvalid = true;
            ireply.cachepc = irequest.address;
        }
        else    // not found or invalid
        {
            ////simul(sim->icachedata.miss++;)
            select(ictrl);
            ictrl.setctrl.tag[ictrl.currentway] = getTag(irequest.address);

            ictrl.state = ICacheControl::Fetch;
            ictrl.setctrl.valid[ictrl.currentway] = false;
            ictrl.i = getOffset(irequest.address);
            ac_int<32, false> wordad = 0;
            wordad.set_slc(0, irequest.address.slc<30>(2));
            wordad.set_slc(30, (ac_int<2, false>)0);
            ictrl.valuetowrite = imem[wordad];
            ////simul(sim->icachedata.mainmemread++;)
            ictrl.memcnt = 1;
            // critical word first
            ireply.instruction = ictrl.valuetowrite;

            insert_policy(ictrl);

            ireply.insvalid = false;
        }
        break;
    case ICacheControl::StoreControl:
        #pragma hls_unroll yes
        if(ictrl.ctrlLoaded)        // this prevent storing false control when we jump to another jump ireply.instruction
        {
            ac_int<IWidth, false> setctrl = 0;
            setctrl.set_slc(ICacheControlWidth, ictrl.setctrl.bourrage);
            #pragma hls_unroll yes
            storeicontrol:for(int i = 0; i < Associativity; ++i)
            {
                setctrl.set_slc(i*(32-tagshift), ictrl.setctrl.tag[i]);
                setctrl.set_slc(Associativity*(32-tagshift) + i, (ac_int<1, false>)ictrl.setctrl.valid[i]);
            }
        #if Associativity > 1 && (Policy == RP_FIFO || Policy == RP_LRU)
            setctrl.set_slc(Associativity*(32-tagshift+1), ictrl.setctrl.policy);
        #endif
            memictrl[ictrl.currentset] = setctrl;
            ////simul(sim->icachedata.ctrlmemwrite++;)
        }

        ictrl.state = ICacheControl::Idle;
        ictrl.currentset = getSet(irequest.address);  //use workaddress?
        ictrl.workAddress = irequest.address;
        ictrl.ctrlLoaded = false;
        ireply.insvalid = false;
        break;
    case ICacheControl::Fetch:
        if(ictrl.memcnt == MEMORY_READ_LATENCY)
        {
            data[ictrl.currentset][ictrl.i][ictrl.currentway] = ictrl.valuetowrite;
            //simul(sim->icachedata.cachememwrite++;)
            ireply.instruction = ictrl.valuetowrite;
            ireply.cachepc = ictrl.workAddress;
            ireply.cachepc.set_slc(2, ictrl.i);
            ireply.insvalid = true;
            //simul(sim->icachedata.hit += ictrl.workAddress == irequest.address;)

            if(++ictrl.i != getOffset(ictrl.workAddress))
            {
                ac_int<32, false> bytead = 0;
                setTag(bytead, ictrl.setctrl.tag[ictrl.currentway]);
                setSet(bytead, ictrl.currentset);
                setOffset(bytead, ictrl.i);

                ictrl.valuetowrite = imem[bytead >> 2];
                //simul(sim->icachedata.mainmemread++;)
            }
            else
            {
                ictrl.state = ICacheControl::Idle;
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
        ictrl.state = ICacheControl::Idle;
        ictrl.ctrlLoaded = false;
        break;
    }

}

void dcache(ac_int<DWidth, false> memdctrl[Sets], unsigned int dmem[DRAM_SIZE], // control & memory
            unsigned int data[Sets][Blocksize][Associativity],                  // cachedata
            DCacheRequest drequest, DCacheReply& dreply                         // from & to cpu
        #ifndef __HLS__
            , Simulator* sim
        #endif
            )
{
    /*if(dcacheenable && datavalid)   // we can avoid storing control if we hit same set multiple times in a row
    {
        if(dctrl.currentset != getSet(address))
        {
            dctrl.state == DCacheControl::StoreControl;
        }
        else
        {
            dctrl.state == DCacheControl::Idle;
        }
    }
    else if(!dcacheenable && datavalid)
    {
        dctrl.state == DCacheControl::StoreControl;
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
    case DCacheControl::Idle:
        if(dcacheenable)
        {
            dctrl.currentset = getSet(address);
            dctrl.i = getOffset(address);

            ac_int<DWidth, false> setctrl = memdctrl[dctrl.currentset];
            //simul(sim->dcachedata.ctrlmemread++;)
            dctrl.setctrl.bourrage = setctrl.slc<dbourrage>(DCacheControlWidth);
            #pragma hls_unroll yes
            loaddset:for(int i = 0; i < Associativity; ++i)
            {
                dctrl.setctrl.data[i] = data[dctrl.currentset][dctrl.i][i];

                dctrl.setctrl.tag[i] = setctrl.slc<32-tagshift>(i*(32-tagshift));
                dctrl.setctrl.valid[i] = setctrl.slc<1>(Associativity*(32-tagshift) + i);
                dctrl.setctrl.dirty[i] = setctrl.slc<1>(Associativity*(32-tagshift+1) + i);
            }
            //simul(sim->dcachedata.cachememread+=Associativity;) // we have A bank, so + Associativity
        #if Associativity > 1 && (Policy == RP_FIFO || Policy == RP_LRU)
            dctrl.setctrl.policy = setctrl.slc<DPolicyWidth>(Associativity*(32-tagshift+2));
        #endif

            if(find(dctrl, address))
            {
                //simul(sim->dcachedata.hit++;)
                if(writeenable)
                {
                    dctrl.valuetowrite = dctrl.setctrl.data[dctrl.currentway];
                    formatwrite(address, datasize, dctrl.valuetowrite, writevalue);
                    dctrl.workAddress = address;
                    dctrl.setctrl.dirty[dctrl.currentway] = true;

                    dctrl.state = DCacheControl::StoreData;
                }
                else
                {
                    ac_int<32, false> r = dctrl.setctrl.data[dctrl.currentway];
                    formatread(address, datasize, signenable, r);

                    read = r;

                    dctrl.state = DCacheControl::StoreControl;
                }
                update_policy(dctrl);
                datavalid = true;
            }
            else    // not found or invalid
            {
                //simul(sim->dcachedata.miss++;)
                select(dctrl);
                if(dctrl.setctrl.dirty[dctrl.currentway] && dctrl.setctrl.valid[dctrl.currentway])
                {
                    dctrl.state = DCacheControl::FirstWriteBack;
                    dctrl.i = 0;
                    dctrl.workAddress = 0;
                    setTag(dctrl.workAddress, dctrl.setctrl.tag[dctrl.currentway]);
                    setSet(dctrl.workAddress, dctrl.currentset);
                    //dctrl.valuetowrite = dctrl.setctrl.data[dctrl.currentway];    // only if same offset than requested address
                    datavalid = false;
                }
                else
                {
                    dctrl.setctrl.tag[dctrl.currentway] = getTag(address);
                    dctrl.workAddress = address;
                    dctrl.state = DCacheControl::Fetch;
                    dctrl.setctrl.valid[dctrl.currentway] = false;
                    dctrl.i = getOffset(address);
                    ac_int<32, false> wordad = 0;
                    wordad.set_slc(0, address.slc<30>(2));
                    wordad.set_slc(30, (ac_int<2, false>)0);
                    dctrl.valuetowrite = dmem[wordad];
                    //simul(sim->dcachedata.mainmemread++;)
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
    case DCacheControl::StoreControl:
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

        //simul(sim->dcachedata.ctrlmemwrite++;)
        memdctrl[dctrl.currentset] = setctrl;
        dctrl.state = DCacheControl::Idle;
        datavalid = false;
        break;
    }
    case DCacheControl::StoreData:
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
        //simul(sim->dcachedata.ctrlmemwrite++;)

        data[dctrl.currentset][getOffset(dctrl.workAddress)][dctrl.currentway] = dctrl.valuetowrite;
        //simul(sim->dcachedata.cachememwrite++;)

        dctrl.state = DCacheControl::Idle;
        datavalid = false;
        break;
    }
    case DCacheControl::FirstWriteBack:
    {   //bracket for scope and allow compilation
        dctrl.i = 0;
        dctrl.memcnt = 0;
        ac_int<32, false> bytead = 0;
        setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
        setSet(bytead, dctrl.currentset);
        setOffset(bytead, dctrl.i);

        dctrl.valuetowrite = data[dctrl.currentset][dctrl.i][dctrl.currentway];
        //simul(sim->dcachedata.cachememread++;)
        dctrl.state = DCacheControl::WriteBack;
        datavalid = false;
        break;
    }
    case DCacheControl::WriteBack:
        if(dctrl.memcnt == MEMORY_WRITE_LATENCY)
        {   //bracket for scope and allow compilation
            ac_int<32, false> bytead = 0;
            setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
            setSet(bytead, dctrl.currentset);
            setOffset(bytead, dctrl.i);
            dmem[bytead >> 2] = dctrl.valuetowrite;
            //simul(sim->dcachedata.mainmemwrite++;)

            if(++dctrl.i)
            {
                dctrl.valuetowrite = data[dctrl.currentset][dctrl.i][dctrl.currentway];
                //simul(sim->dcachedata.cachememread++;)
            }
            else
            {
                dctrl.state = DCacheControl::StoreControl;
                dctrl.setctrl.dirty[dctrl.currentway] = false;
            }
            dctrl.memcnt = 0;
        }
        else
        {
            dctrl.memcnt++;
        }
        datavalid = false;
        break;
    case DCacheControl::Fetch:
        if(dctrl.memcnt == MEMORY_READ_LATENCY)
        {
            data[dctrl.currentset][dctrl.i][dctrl.currentway] = dctrl.valuetowrite;
            //simul(sim->dcachedata.cachememwrite++;)

            if(++dctrl.i != getOffset(dctrl.workAddress))
            {
                ac_int<32, false> bytead = 0;
                setTag(bytead, dctrl.setctrl.tag[dctrl.currentway]);
                setSet(bytead, dctrl.currentset);
                setOffset(bytead, dctrl.i);

                dctrl.valuetowrite = dmem[bytead >> 2];
                //simul(sim->dcachedata.mainmemread++;)
            }
            else
            {
                dctrl.state = DCacheControl::StoreControl;
                dctrl.setctrl.valid[dctrl.currentway] = true;
                update_policy(dctrl);
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
        dctrl.state = DCacheControl::Idle;
        break;
    }

    dreply.readvalue = read;
    dreply.datavalid = datavalid;
}
