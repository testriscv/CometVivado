#include "multicycleoperator.h"

#ifndef __HLS__
#include "simulator.h"
#endif

void multicyclecontroller(MultiCycleOp mcop, MultiCycleRes& mcres
                        #ifndef __HLS__
                          , Simulator* sim
                        #endif
                          )
{
    static int multicycle = 0;
    if(multicycle)
    {
        multicycle -= 1;
        if(multicycle == 0)
            mcres.done = true;
    }
    else if(mcop.op != MultiCycleOperation::NONE)
    {   // put this in a function that can be synthesized
        mcres.done = false;
        ac_int<32, true> lhs = mcop.lhs;
        ac_int<32, true> rhs = mcop.rhs;
        multicycle = 3;
        switch(mcop.op)
        {
        case MultiCycleOperation::DIV:
            if(rhs)
                mcres.res = lhs / rhs;
            else
                mcres.res = -1;
            simul(sim->coredata.div[0]++;)
            break;
        case MultiCycleOperation::DIVU:
            if(rhs)
                mcres.res = (ac_int<32, false>)lhs / (ac_int<32, false>)rhs;
            else
                mcres.res = -1;
            simul(sim->coredata.div[1]++;)
            break;
        case MultiCycleOperation::REM:
            if(rhs)
                mcres.res = lhs % rhs;
            else
                mcres.res = lhs;
            simul(sim->coredata.rem[0]++;)
            break;
        case MultiCycleOperation::REMU:
            if(rhs)
                mcres.res = (ac_int<32, false>)lhs % (ac_int<32, false>)rhs;
            else
                mcres.res = lhs;
            simul(sim->coredata.rem[1]++;)
            break;
        default:
            dbgassert(false, "Unknown external operation @%06x\n", mcop.pc.to_int());
            break;
        }
    }
    else
    {
        mcres.done = false;
        mcres.res = 0;
        //mcres.rd = 0;
    }
}
