#include "multicycleoperator.h"

#ifndef __HLS__
#include "simulator.h"
#endif

void multicyclecontroller(MultiCycleOperator op, MultiCycleRes& res
                        #ifndef __HLS__
                          , Simulator* sim
                        #endif
                          )
{
    static int multicycle = 0;
    MultiCycleOperator mcop = op;
    static MultiCycleRes mcres;
    if(multicycle)
    {
        multicycle -= 1;
        if(multicycle == 0)
            mcres.done = true;
    }
    else if(mcop.op != MultiCycleOperator::NONE)
    {   // put this in a function that can be synthesized
        mcres.done = false;
        ac_int<33, true> lhs = 0;
        ac_int<33, true> rhs = 0;
        lhs.set_slc(0, mcop.lhs);
        rhs.set_slc(0, mcop.rhs);
        switch(mcop.op)
        {
        case MultiCycleOperator::DIVU:
            lhs[32] = 0;
            rhs[32] = 0;
            simul(sim->coredata.div[1]++;)
            multicycle = 3;
            break;
        case MultiCycleOperator::REMU:
            lhs[32] = 0;
            rhs[32] = 0;
            simul(sim->coredata.rem[1]++;)
            multicycle = 4;
            break;
        case MultiCycleOperator::DIV:
            simul(sim->coredata.div[0]++;)
            multicycle = 3;
            break;
        case MultiCycleOperator::REM:
            simul(sim->coredata.rem[0]++;)
            multicycle = 4;
            break;
        }
        
        if(mcop.op == MultiCycleOperator::DIV || mcop.op == MultiCycleOperator::DIVU)
        {   
            if(rhs)
                mcres.res = lhs / rhs;
            else
                mcres.res = -1;
        }
        else if(mcop.op == MultiCycleOperator::REM || mcop.op == MultiCycleOperator::REMU)
        {
            if(rhs)
                mcres.res = lhs % rhs;
            else
                mcres.res = lhs;
        }
        else
            dbgassert(false, "Unknown external operation @%06x\n", mcop.pc.to_int());
    }
    else
    {
        mcres.done = false;
        mcres.res = 0;
        //mcres.rd = 0;
    }
    
    res = mcres;
}
