////////////////////////////////////////////////////////////////////////////////
// Catapult Synthesis
// 
// Copyright (c) 2003-2011 Mentor Graphics Corp.
//       All Rights Reserved
// 
// This document contains information that is proprietary to Mentor Graphics
// Corp. The original recipient of this document may duplicate this  
// document in whole or in part for internal business purposes only, provided  
// that this entire notice appears in all copies. In duplicating any part of  
// this document, the recipient agrees to make every reasonable effort to  
// prevent the unauthorized use and distribution of the proprietary information.
//
////////////////////////////////////////////////////////////////////////////////

/****************************************************************************
  ===========================================================================
  Source:         mgc_ac_math.h
  Description:    synthesizable math functions for ac_int 
  ===========================================================================

  Overview: the functions supplied here have been parameterized so that
    the precision of the input(s) and the output(s) determine the precision
    of the computation and thus the hardware that Catapult C will synthesize.
    These functions are generic (e.g, radix 2 division) and are meant to 
    provide flexibility in trading off performance and area by unrolling the
    iteration loop.  
     
  Datatypes: the functions support the Algorithmic C datatypes:
    - ac_int
    - ac_fixed
        + inputs: any rounding and overflow mode
        + outputs: any rounding/overflow modes

  Functions currently supported:
    - Division:
        + inputs: dividend, divisor
        + outputs: quotient, returns true if remainder is nonzero (for
            fixed point, the remainder depends on the LSB precision of
            the quotient).
            For integer divisions, remainder output is also available.

        * Integer Unsigned
          bool div(ac_int<NW,false> dividend, ac_int<DW,false> divisor,
                   ac_int<QW,false> &quotient, ac_int<RW,false> &remainder)
          bool div(ac_int<NW,false> dividend, ac_int<DW,false> divisor,
                   ac_int<QW,false> &quotient)

        * Integer Signed
          bool div(ac_int<NW,true> dividend, ac_int<DW,true> divisor,
                   ac_int<QW,true> &quotient, ac_int<RW,true> &remainder)
          bool div(ac_int<NW,true> dividend, ac_int<DW,true> divisor,
                   ac_int<QW,true> &quotient)

        * Fixed Point unsigned
          bool div(ac_fixed<NW,NI,false,NQ,NO> dividend,
                   ac_fixed<DW,DI,false,DQ,DO> divisor,
                   ac_fixed<QW,QI,false,QQ,QO> &quotient)

        * Fixed Point signed
          bool div(ac_fixed<NW,NI,true,NQ,NO> dividend,
                   ac_fixed<DW,DI,true,DQ,DO> divisor,
                   ac_fixed<QW,QI,true,QQ,QO> &quotient)

    - Square Root:
        + inputs: argument
        + outputs: square root

        * Integer Unsigned
          void sqrt(ac_int<XW,false> x, ac_int<OW,false> &sqrt)

        * Fixed Point Unsigned
          void sqrt(ac_fixed<XW,XI,false,XQ,XO> x, ac_fixed<OW,OI,false,OQ,OO> &sqrt)

    - Shifts:
        + inputs: argument, shift count
        + output: shifted result

        * Fixed Point Unsigned, signed shift value
          void shift_right(ac_fixed<XW,XI,false,XQ,XO> x, int n,
                           ac_fixed<OW,OI,false,OQ,OO> &sr)
          void shift_left(ac_fixed<XW,XI,false,XQ,XO> x, int n,
                          ac_fixed<OW,OI,false,OQ,OO> &sl)

        * Fixed Point Unsigned, unsigned shift value
          void shift_right(ac_fixed<XW,XI,false,XQ,XO> x, unsigned int n,
                           ac_fixed<OW,OI,false,OQ,OO> &sr)
          void shift_left(ac_fixed<XW,XI,false,XQ,XO> x, unsigned int n,
                          ac_fixed<OW,OI,false,OQ,OO> &sl)

        * Fixed Point Signed, signed shift value
          void shift_right(ac_fixed<XW,XI,true,XQ,XO> x, int n,
                           ac_fixed<OW,OI,true,OQ,OO> &sr)
          void shift_left(ac_fixed<XW,XI,true,XQ,XO> x, int n,
                          ac_fixed<OW,OI,true,OQ,OO> &sl)

        * Fixed Point Signed, unsigned shift value
          void shift_right(ac_fixed<XW,XI,true,XQ,XO> x, unsigned int n,
                           ac_fixed<OW,OI,true,OQ,OO> &sr)
          void shift_left(ac_fixed<XW,XI,true,XQ,XO> x, unsigned int n,
                          ac_fixed<OW,OI,true,OQ,OO> &sl)

    - Sin/Cos:
        sin(angle_over_pi*PI):
          void sin(ac_fixed<AW,AI,true,AQ,AO> angle_over_pi,
                   ac_fixed<OW,OI,true,OQ,OO> &sin)

        cos(angle_over_pi*PI):
          void cos(ac_fixed<AW,AI,true,AQ,AO> angle_over_pi,
                   ac_fixed<OW,OI,true,OQ,OO> &cos)

        C*sin(angle_over_pi*PI), C*cos(angle_over_pi*PI):
          void sin_cos(ac_fixed<AW,AI,true,AQ,AO> angle_over_pi,
                       ac_fixed<OW,OI,true,OQ,OO> C,
                       ac_fixed<OW,OI,true,OQ,OO> &sin,
                       ac_fixed<OW,OI,true,OQ,OO> &cos)

    - Atan2: similar functionality as math.h atan2(y,x)
        void atan2(ac_fixed<YW,YI,true,YQ,YO> y,
                   ac_fixed<XW,XI,true,XQ,XO> x,
                   ac_fixed<OW,OI,true,OQ,OO> &atan)

  ===========================================================================

  Notes on Compilation: if during compilation you get errors about
  "no matching functions" check the following:
    - matching datatype (integer or fixed point, signed or unsigned) for
      inputs and outputs
    - when using sin_cos, the input argument C and output arguments
      sin and cos should have the same datatype

  ===========================================================================

  Notes on Simulation: div/sqrt/shift functions are bitwise accurate with
  the correspoding math.h (double) function provided the datatypes dont
  go beyond the precision of double. For instance,

     ac_fixed<20,6,true> n = ...;
     ac_fixed<5,0,true> d = ...;
     ac_fixed<40,14,true> q, q1;
     div(n, d, q);
     q1 = (double)n/(double)/d;
     assert(q == q1);

  Transcendental functions (e.g, trigonometric) will not be bit accurate
  compared with the math.h (double) function. The difference should be
  confined to the quantization error of the requested output.

  ===========================================================================

  Notes on Synthesis: constraining the precision for both inputs and outputs
  will deliver the best quality of results.

****************************************************************************/

#ifndef __MGC_AC_MATH_H
#define __MGC_AC_MATH_H

#ifndef __cplusplus
#error C++ is required to include this header file
#endif

#include <ac_fixed.h>

//////////////////////////////////////////////////////////////////////////////
//                              I N T E G E R
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Division 
//////////////////////////////////////////////////////////////////////////////

template< int NW, int DW, int QW, int RW > 
bool div(
  ac_int<NW,false> dividend,
  ac_int<DW,false> divisor,
  ac_int<QW,false> &quotient,
  ac_int<RW,false> &remainder
){
  ac_int<QW,false> Q = 0;
  ac_int<DW+1,true> R = 0; 
  ac_int<DW+1,true> neg_divisor = -divisor;
  for(int i=0; i < NW; i++) {
    // take MSB of divd, shift it in from right to R
    R = ((R << 1) | ((dividend >> (NW-1)) & 1)) + 
      ( (R < 0) ? (ac_int<DW+1,true>) divisor : neg_divisor );
    Q = (Q << 1) | ((R >= 0) & 1);
    dividend <<= 1;
  }
  if(R < 0)
    R += divisor;
  quotient = Q;
  remainder = R;
  return !! R;
}

template< int NW, int DW, int QW > 
bool div(
  ac_int<NW,false> dividend,
  ac_int<DW,false> divisor,
  ac_int<QW,false> &quotient
){
  ac_int<DW, false> remainder;
  div(dividend, divisor, quotient, remainder);
  return !! remainder;
}

template< int NW, int DW, int QW, int RW >
bool div(
  ac_int<NW,true> dividend,
  ac_int<DW,true> divisor,
  ac_int<QW,true> &quotient,
  ac_int<RW,true> &remainder
){
  bool neg_dividend = dividend < 0;
  ac_int<NW,false> uN = neg_dividend ? (ac_int<NW,false>) -dividend : (ac_int<NW,false>) dividend;
  bool neg_divisor = divisor < 0;
  ac_int<DW,false> uD = neg_divisor ? (ac_int<DW,false>) -divisor : (ac_int<DW,false>) divisor;
  ac_int<QW,false> uQ;
  ac_int<DW,false> uR; 
  div(uN, uD, uQ, uR);
  quotient = neg_dividend == neg_divisor ? (ac_int<QW,true>) uQ : (ac_int<QW,true>) -uQ; 
  ac_int<RW,true> rem = neg_dividend ? (ac_int<DW,true>) -uR : (ac_int<DW,true>) uR;
  remainder = rem;
  return !! rem;
}

template< int NW, int DW, int QW >
bool div(
  ac_int<NW,true> dividend,
  ac_int<DW,true> divisor,
  ac_int<QW,true> &quotient
){
  ac_int<DW,true> remainder;
  div(dividend, divisor, quotient, remainder);
  return !! remainder;
}

//////////////////////////////////////////////////////////////////////////////
// Square Root 
//////////////////////////////////////////////////////////////////////////////

template< int XW, int OW, bool OS >
void sqrt(
  ac_int<XW,false> x,
  ac_int<OW,OS> &sqrt
) {
  const int RW = (XW+1)/2;
  // masks used only to hint synthesis on precision
  ac_int<RW+2, false> mask_d = 0;

  ac_int<RW+2, false> d = 0;
  ac_int<RW, false> r = 0;
  ac_int<2*RW, false> z = x; 

  // needs to pick 2 bits of z for each iteration starting from
  // the 2 MSB bits. Inside loop, z will be shifted left by 2 each
  // iteration. The bits of interest are always on the same
  // position (z_shift+1 downto z_shift)
  unsigned int z_shift = (RW-1)*2;

  for(int i = RW-1; i >= 0; i--) {
    r <<= 1;

    mask_d = (mask_d << 2) | 0x3;
    d = mask_d & (d << 2) | ((z >> z_shift) & 0x3 );

    ac_int<RW+2, false> t = d - (( ((ac_int<RW+1, false>)r) << 1) | 0x1);
    if( !t[RW+1] ) {  // since t is unsigned, look at MSB
      r |= 0x1;
      d = mask_d & t;
    }
    z <<= 2;
  }
  sqrt = r;
}

//////////////////////////////////////////////////////////////////////////////
//                         F I X E D   P O I N T
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Shifts
//////////////////////////////////////////////////////////////////////////////

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_right(ac_fixed<XW,XI,false,XQ,XO> x, int n, ac_fixed<OW,OI,false,OQ,OO> &sr) {
  const int R_BIT = (OQ == AC_TRN || OQ == AC_TRN_ZERO) ? 0 : 1;
  const int R_HALF = (R_BIT == 0 || OQ == AC_RND || OQ == AC_RND_INF) ? 0 : 1;
  const int S_OVER = (OO == AC_WRAP) ? 0 : 1;
  const int TF = AC_MAX(XW-XI, OW-OI+R_BIT);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  ac_fixed<TW,TI,false> t = ((ac_fixed<TW,TI,false>) x) >> n;

  ac_fixed<TW+R_HALF+S_OVER,TI+S_OVER,false> t2 = t;

  if(R_HALF || S_OVER) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,false>) 0;
    m1 <<= n;
    ac_fixed<XW,XI,false> mask = ~(ac_fixed<XW,XI,false>) m1; 
    if(n >= 0) {
      if(R_HALF)
        t2[0] = !! (x & mask);
    }
    else {
      if(S_OVER)
        t2[TW+R_HALF+S_OVER-1] = !! (x & mask);
    }
  }
  sr = t2;
}

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_right(ac_fixed<XW,XI,false,XQ,XO> x, unsigned int n, ac_fixed<OW,OI,false,OQ,OO> &sr) {
  const int R_BIT = (OQ == AC_TRN || OQ == AC_TRN_ZERO) ? 0 : 1;
  const int R_HALF = (R_BIT == 0 || OQ == AC_RND || OQ == AC_RND_INF) ? 0 : 1;
  const int TF = AC_MAX(XW-XI, OW-OI+R_BIT);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  unsigned un = 0x7FFFFFFF & n;
  ac_fixed<TW,TI,false> t = ((ac_fixed<TW,TI,false>) x) >> un;

  ac_fixed<TW+R_HALF,TI,false> t2 = t;

  if(R_HALF) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,false>) 0;
    m1 <<= un;
    ac_fixed<XW,XI,false> mask = ~(ac_fixed<XW,XI,false>)  m1;
    t2[0] = !! (x & mask);
  }
  sr = t2;
}

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_right(ac_fixed<XW,XI,true,XQ,XO> x, int n, ac_fixed<OW,OI,true,OQ,OO> &sr) {
  const int R_BIT = (OQ == AC_TRN) ? 0 : 1;
  const int R_HALF = (R_BIT == 0 || OQ == AC_RND) ? 0 : 1;
  const int S_OVER = (OO == AC_WRAP) ? 1 : 2;
  const int TF = AC_MAX(XW-XI, OW-OI+R_BIT);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  ac_fixed<TW,TI,true> t = ((ac_fixed<TW,TI,true>) x) >> n;

  ac_fixed<TW+R_HALF+S_OVER,TI+S_OVER,true> t2 = t;

  if(R_HALF || S_OVER==2) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,true>) 0; 
    m1 <<= n;
    ac_fixed<XW,XI,true> mask = ~(ac_fixed<XW,XI,true>) m1;
    if(n >= 0) {
      if(R_HALF)
        t2[0] = !! (x & mask);
    }
    else {
      t2[TW+R_HALF+S_OVER-1] = x[XW-1];
      if(S_OVER==2 && !! mask )
        t2[TW+R_HALF+S_OVER-2] = !!(x & mask) && !(!!(~x & mask) && x[XW-1]);
    }
  }
  sr = t2;
}

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_right(ac_fixed<XW,XI,true,XQ,XO> x, unsigned int n, ac_fixed<OW,OI,true,OQ,OO> &sr) {
  const int R_BIT = (OQ == AC_TRN) ? 0 : 1;
  const int R_HALF = (R_BIT == 0 || OQ == AC_RND) ? 0 : 1;
  const int TF = AC_MAX(XW-XI, OW-OI+R_BIT);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  ac_fixed<TW,TI,true> t = ((ac_fixed<TW,TI,true>) x) >> n;
  unsigned un = 0x7FFFFFFF & n;

  ac_fixed<TW+R_HALF,TI,true> t2 = t;

  if(R_HALF) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,false>) 0;
    m1 <<= un;
    ac_fixed<XW,XI,true> mask = ~(ac_fixed<XW,XI,true>) m1; 
    t2[0] = !! (x & mask);
  }
  sr = t2;
}
                                                                                                                                             
template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_left(ac_fixed<XW,XI,false,XQ,XO> x, int n, ac_fixed<OW,OI,false,OQ,OO> &sl) {
  const int R_BIT = (OQ == AC_TRN || OQ == AC_TRN_ZERO) ? 0 : 1;
  const int R_HALF = (R_BIT == 0 || OQ == AC_RND || OQ == AC_RND_INF) ? 0 : 1;
  const int S_OVER = (OO == AC_WRAP) ? 0 : 1;
  const int TF = AC_MAX(XW-XI, OW-OI+R_BIT);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  ac_fixed<TW,TI,false> t = ((ac_fixed<TW,TI,false>) x) << n;

  ac_fixed<TW+R_HALF+S_OVER,TI+S_OVER,false> t2 = t;

  if(R_HALF || S_OVER) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,false>) 0;
    m1 >>= n;
    ac_fixed<XW,XI,false> mask = ~(ac_fixed<XW,XI,false>) m1; 
    if(n < 0) {
      if(R_HALF)
        t2[0] = !! (x & mask);
    }
    else {
      if(S_OVER)
        t2[TW+R_HALF+S_OVER-1] = !! (x & mask);
    }
  }
  sl = t2;
}

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_left(ac_fixed<XW,XI,false,XQ,XO> x, unsigned int n, ac_fixed<OW,OI,false,OQ,OO> &sl) {
  const int S_OVER = (OO == AC_WRAP) ? 0 : 1;
  const int TF = AC_MAX(XW-XI, OW-OI);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  ac_fixed<TW,TI,false> t = ((ac_fixed<TW,TI,false>) x) << n;

  ac_fixed<TW+S_OVER,TI+S_OVER,false> t2 = t;

  if(S_OVER) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,false>) 0;
    m1 >>= n;
    ac_fixed<XW,XI,false> mask = ~(ac_fixed<XW,XI,false>) m1; 
    t2[TW+S_OVER-1] = !! (x & mask);
  }
  sl = t2;
}

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_left(ac_fixed<XW,XI,true,XQ,XO> x, int n, ac_fixed<OW,OI,true,OQ,OO> &sl) {
  const int R_BIT = (OQ == AC_TRN) ? 0 : 1;
  const int R_HALF = (R_BIT == 0 || OQ == AC_RND) ? 0 : 1;
  const int S_OVER = (OO == AC_WRAP) ? 1 : 2;
  const int TF = AC_MAX(XW-XI, OW-OI+R_BIT);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  ac_fixed<TW,TI,true> t = ((ac_fixed<TW,TI,true>) x) << n;

  ac_fixed<TW+R_HALF+S_OVER,TI+S_OVER,true> t2 = t;

  if(R_HALF || S_OVER==2) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,false>) 0;
    m1 >>= n;
    ac_fixed<XW,XI,true> mask = ~(ac_fixed<XW,XI,true>) m1; 
    if(n < 0) {
      if(R_HALF)
        t2[0] = !! (x & mask);
    }
    else {
      t2[TW+R_HALF+S_OVER-1] = x[XW-1];
      if(S_OVER==2 && !! mask )
        t2[TW+R_HALF+S_OVER-2] = !!(x & mask) && !(!!(~x & mask) && x[XW-1]);
    }
  }
  sl = t2;
}

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void shift_left(ac_fixed<XW,XI,true,XQ,XO> x, unsigned int n, ac_fixed<OW,OI,true,OQ,OO> &sl) {
  const int S_OVER = (OO == AC_WRAP) ? 1 : 2;
  const int TF = AC_MAX(XW-XI, OW-OI);
  const int TI = AC_MAX(XI, OI);
  const int TW = TI + TF;
  ac_fixed<TW,TI,true> t = ((ac_fixed<TW,TI,true>) x) << n;

  ac_fixed<TW+S_OVER,TI+S_OVER,true> t2 = t;

  if(S_OVER==2) {
    ac_fixed<TW,TI,false> m1 = ~(ac_fixed<TW,TI,false>) 0;
    m1 >>= n;
    ac_fixed<XW,XI,true> mask = ~(ac_fixed<XW,XI,true>) m1; 
    t2[TW+S_OVER-1] = x[XW-1];
    if(mask != 0)
      t2[TW+S_OVER-2] = !!(x & mask) && !(!!(~x & mask) && x[XW-1]);
  }
  sl = t2;
}

//////////////////////////////////////////////////////////////////////////////
// Division 
//////////////////////////////////////////////////////////////////////////////

template< int NW, int NI, ac_q_mode NQ, ac_o_mode NO,
          int DW, int DI, ac_q_mode DQ, ac_o_mode DO,
          int QW, int QI, ac_q_mode QQ, ac_o_mode QO >
bool div(
  ac_fixed<NW,NI,false,NQ,NO> dividend,
  ac_fixed<DW,DI,false,DQ,DO> divisor,
  ac_fixed<QW,QI,false,QQ,QO> &quotient
){
  // relevant bits for Q

  const int RBIT = (QQ == AC_TRN || QQ == AC_TRN_ZERO) ? 0 : 1;
  const int QF = (QW-QI) + RBIT;
  const int ZI = NI+(DW-DI);
  if(ZI-1 < -QF) {
    // MSB of result is smaller than LSB of requested output
    quotient = 0;
    return !! dividend;
  }
  // max is to used to avoid compilation problems with non pos bitwidth
  const int ZF = AC_MAX(QF, -ZI+1);
  const int ZW = ZI + ZF;

  ac_fixed<NW,NW,false> N_fx;
  shift_left(dividend, NW-NI, N_fx);
  ac_int<NW,false> N = N_fx.template slc<NW>(0);
  ac_fixed<DW,DW,false> D_fx;
  shift_left(divisor, DW-DI, D_fx);
  ac_int<DW,false> D = D_fx.template slc<DW>(0);

  ac_int<ZW,false> Q = 0;
  ac_int<DW+1,true> R = 0;
  ac_int<DW+1,true> neg_D = -D;
  for(int i=0; i < ZW; i++) {
    // take MSB of N, shift it in from right to R
    R = ((R << 1) | ((N >> (NW-1)) & 1)) +
      ( (R < 0) ? (ac_int<DW+1,true>) D : neg_D );
    Q = (Q << 1) | ((R >= 0) & 1);
    N <<= 1;
  }
  if(R < 0)
    R += D;

  bool rem = (R != 0) || ((N >> ZW) != 0);

  ac_fixed<ZW+1,ZW,false> Q_fx = (ac_fixed<ZW+1,ZW,false>)Q;
  if(QQ == AC_RND_ZERO || QQ == AC_RND_MIN_INF || QQ == AC_RND_CONV || QQ == AC_RND_CONV_ODD)
    Q_fx[0] = rem;

  shift_right(Q_fx, (NW-NI) - (DW-DI) + (ZW-NW), quotient);
  return rem || (RBIT && Q[0]);
}

template< int NW, int NI, ac_q_mode NQ, ac_o_mode NO,
          int DW, int DI, ac_q_mode DQ, ac_o_mode DO,
          int QW, int QI, ac_q_mode QQ, ac_o_mode QO >
bool div(
  ac_fixed<NW,NI,true,NQ,NO> dividend,
  ac_fixed<DW,DI,true,DQ,DO> divisor,
  ac_fixed<QW,QI,true,QQ,QO> &quotient
){
  const int ZI = (QO == AC_WRAP) ? QI : NI+(DW-DI);
  const int ZW = ZI+(QW-QI);

  ac_fixed<NW,NI,true> N = dividend;
  ac_fixed<DW,DI,true> D = divisor;

  bool neg_N = N < 0;
  ac_fixed<NW,NI,false> uN = neg_N ? (ac_fixed<NW,NI,false>) -N : (ac_fixed<NW,NI,false>) N;
  bool neg_D = D < 0;
  ac_fixed<DW,DI,false> uD = neg_D ? (ac_fixed<DW,DI,false>) -D : (ac_fixed<DW,DI,false>) D;

  bool has_rem;
  if( QQ == AC_RND_ZERO || QQ == AC_RND_INF ||
      QQ == AC_RND_CONV || QQ == AC_RND_CONV_ODD || QQ == AC_TRN_ZERO ) {
    ac_fixed<ZW,ZI,false,QQ> uQ;
    ac_fixed<ZW+1,ZI+1,true,QQ> Q;
    has_rem = div(uN, uD, uQ);
    if(neg_N == neg_D)
      Q = uQ;
    else
      Q = -uQ;
    quotient = Q;
  }
  else {
    const int RBIT = (QQ == AC_TRN) ? 0 : 1;
    ac_fixed<ZW+RBIT,ZI,false> uQ;
    ac_fixed<ZW+RBIT+2,ZI+1,true> Q;
    has_rem = div(uN, uD, uQ);
    if(neg_N == neg_D) {
      Q = uQ;
      if(QQ == AC_RND_MIN_INF)
        Q[0] = has_rem ? 1 : 0;
    }
    else {
      ac_fixed<ZW+RBIT,ZI,false> lsb = 0;
      lsb[0] = has_rem && QQ != AC_RND_MIN_INF ? 1 : 0;
      Q = -(uQ + lsb);
    }
    quotient = Q;
    has_rem |= RBIT && uQ[0];
  }
  return has_rem;
}

//////////////////////////////////////////////////////////////////////////////
// Square Root 
//////////////////////////////////////////////////////////////////////////////

template< int XW, int XI, ac_q_mode XQ, ac_o_mode XO,
          int OW, int OI, ac_q_mode OQ, ac_o_mode OO >
void sqrt(
  ac_fixed<XW,XI,false,XQ,XO> x,
  ac_fixed<OW,OI,false,OQ,OO> &sqrt
) {
  const int RBIT = (OQ == AC_TRN || OQ == AC_TRN_ZERO) ? 0 : 1;
  const int OF = (OW-OI) + RBIT;

  const int RI = (XI+1)/2;
  if(RI-1 < -OF) {
    // MSB of result is smaller than LSB of requested output
    sqrt = 0;
    return;
  }

  // max is used to avoid compilation problems with non pos bitwidth
  const int RF = AC_MAX(OF, -RI+1);  // OF may be negative
  const int RW = RI + RF;

  // store relevant bits of x in z
  const int ZF = 2*AC_MIN((XW-XI)/2, RF);
  const int ZW = 2*RI+ZF;
  ac_fixed<ZW,ZW,false> z_fx;
  shift_left(x, ZF, z_fx);
  ac_int<ZW,false> z = z_fx.template slc<ZW>(0);

  // masks used only to hint synthesis on precision
  ac_int<RW+2,false> mask_d = 0;

  ac_int<RW+2,false> d = 0;
  ac_int<RW,false> r = 0;

  // needs to pick 2 bits of z for each iteration starting from
  // the 2 MSB bits. Inside loop, z will be shifted left by 2 each
  // iteration. The bits of interest are always on the same
  // position (z_shift+1 downto z_shift)
  unsigned int z_shift = ZW-2;

  for(int i = RW-1; i >= 0; i--) {
    r <<= 1;

    mask_d = (mask_d << 2) | 0x3;
    d = mask_d & (d << 2) | ((z >> z_shift) & 0x3 );

    ac_int<RW+2,false> t = d - (( ((ac_int<RW+1,false>)r) << 1) | 0x1);
    if( !t[RW+1] ) {  // since t is unsigned, look at MSB
      r |= 0x1;
      d = mask_d & t;
    }
    z <<= 2;
  }

  ac_fixed<RW+1,RW,false> r2 = (ac_fixed<RW+1,RW,false>) r;
  if(OQ == AC_RND_ZERO || OQ == AC_RND_MIN_INF || OQ == AC_RND_CONV ||  OQ == AC_RND_CONV_ODD) {
    bool rem = (d != 0) || ((z >> 2*RW) != 0);
    if(ZF < (XW-XI)) {
      // max is to used to avoid compilation problems with non pos bitwidth
      const int rbits = AC_MAX((XW-XI)-ZF,1);
      ac_fixed<rbits,-ZF,false> zr = x;
      rem |= !! zr;
    }
    r2[0] =  rem ? 1 : 0;
  }
  shift_right(r2, RF, sqrt);

}

//////////////////////////////////////////////////////////////////////////////
// Sine/Cosine/Arctan
//////////////////////////////////////////////////////////////////////////////
                                                                                                                                             
#include "mgc_ac_trig.h"

//////////////////////////////////////////////////////////////////////////////
#endif // __MGC_AC_MATH_H
