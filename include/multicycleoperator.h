#ifndef MULTICYCLEOPERATOR_H
#define MULTICYCLEOPERATOR_H

#include "portability.h"

namespace MultiCycleOperation
{
enum MultiCycleOperation
{
    NONE    ,
    DIV     ,
    DIVU    ,
    REM     ,
    REMU
};
}

struct MultiCycleOp
{
    MultiCycleOp()
    : op(MultiCycleOperation::NONE), lhs(0), rhs(0), pc(0)
    {}

    MultiCycleOperation::MultiCycleOperation op;
    ac_int<32, true> lhs;
    ac_int<32, true> rhs;
    //ac_int<5, false> rd;
    ac_int<32, false> pc;
};

struct MultiCycleRes
{
    MultiCycleRes()
    : done(false), res(0)
    {}

    bool done;
    ac_int<32, true> res;
    //ac_int<5, false> rd;
};

class Simulator;

void multicyclecontroller(MultiCycleOp op, MultiCycleRes& res
                        #ifndef __HLS__
                          , Simulator* sim
                        #endif
                          );

#endif // MULTICYCLEOPERATOR_H
