////////////////////////////////////////////////////////////////////////////////
// Catapult Synthesis
// 
// Copyright (c) 2003-2015 Mentor Graphics Corp.
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

#ifndef __AC_PROBE_H
#define __AC_PROBE_H

#include <string>
#include <ac_int.h>

#if defined(CCS_SCVERIFY) && defined(CCS_DUT_RTL) && !defined(CCS_DUT_SYSC) && !defined(CCS_SYSC)
#include "ccs_probes.h"
#endif

#ifdef __AC_NAMESPACE
namespace __AC_NAMESPACE {
#endif
using std::string;
namespace ac {
  template <class T>
  inline void probe(string prbnm, T val) {
  #if defined(CCS_SCVERIFY) && defined(CCS_DUT_RTL) && !defined(CCS_DUT_SYSC) && !defined(CCS_SYSC)
    enum {thislv = ccs_scverify_probe_traits<T>::lvwidth};
    sc_dt::sc_lv<thislv> tmp;
	 type_to_vector(val,thislv,tmp);
    ccs_probe_fifo_put<thislv>(prbnm, tmp);
  #endif
  }
  #if defined(CCS_SCVERIFY) && defined(CCS_DUT_RTL) && !defined(CCS_DUT_SYSC) && !defined(CCS_SYSC)
  template <class T>
  inline void probe(string prbnm, T* val) {
    enum {thislv = ccs_scverify_probe_traits<T>::lvwidth};
    sc_dt::sc_lv<thislv> tmp;
	 type_to_vector(*val,thislv,tmp);
    ccs_probe_fifo_put<thislv>(prbnm, tmp);
  }
  #endif
}

#ifdef __AC_NAMESPACE
}
#endif

#endif
