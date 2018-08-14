#ifndef __INCLUDED_ST_SPHD_BB_4096x128m8_aTdol_wrapper_trans_rsc_H__
#define __INCLUDED_ST_SPHD_BB_4096x128m8_aTdol_wrapper_trans_rsc_H__
#include <mc_transactors.h>

template < 
  int Bits_Func
  ,int Addr
>
class ST_SPHD_BB_4096x128m8_aTdol_wrapper_trans_rsc : public mc_wire_trans_rsc_base<128,4096>
{
public:
  sc_in< sc_lv<Addr> >   A;
  sc_in< bool >   CK;
  sc_in< sc_logic >   CSN;
  sc_in< sc_lv<Bits_Func> >   D;
  sc_in< sc_logic >   INITN;
  sc_out< sc_lv<Bits_Func> >   Q;
  sc_in< sc_logic >   WEN;

  typedef mc_wire_trans_rsc_base<128,4096> base;
  MC_EXPOSE_NAMES_OF_BASE(base);

  SC_HAS_PROCESS( ST_SPHD_BB_4096x128m8_aTdol_wrapper_trans_rsc );
  ST_SPHD_BB_4096x128m8_aTdol_wrapper_trans_rsc(const sc_module_name& name, bool phase, double clk_skew_delay=0.0)
    : base(name, phase, clk_skew_delay)
    ,A("A")
    ,CK("CK")
    ,CSN("CSN")
    ,D("D")
    ,Q("Q")
    ,WEN("WEN")
    ,_is_connected_port_0(true)
    ,_is_connected_port_0_messaged(false)
  {
    SC_METHOD(at_active_clock_edge);
    sensitive << (phase ? CK.pos() : CK.neg());
    this->dont_initialize();

    MC_METHOD(clk_skew_delay);
    this->sensitive << this->_clk_skew_event;
    this->dont_initialize();
  }

  virtual void start_of_simulation() {
    if ((base::_holdtime == 0.0) && this->get_attribute("CLK_SKEW_DELAY")) {
      base::_holdtime = ((sc_attribute<double>*)(this->get_attribute("CLK_SKEW_DELAY")))->value;
    }
    if (base::_holdtime > 0) {
      std::ostringstream msg;
      msg << "ST_SPHD_BB_4096x128m8_aTdol_wrapper_trans_rsc CLASS_STARTUP - CLK_SKEW_DELAY = "
        << base::_holdtime << " ps @ " << sc_time_stamp();
      SC_REPORT_INFO(this->name(), msg.str().c_str());
    }
    reset_memory();
  }

  virtual void inject_value(int addr, int idx_lhs, int mywidth, sc_lv_base& rhs, int idx_rhs) {
    this->set_value(addr, idx_lhs, mywidth, rhs, idx_rhs);
  }

  virtual void extract_value(int addr, int idx_rhs, int mywidth, sc_lv_base& lhs, int idx_lhs) {
    this->get_value(addr, idx_rhs, mywidth, lhs, idx_lhs);
  }

private:
  void at_active_clock_edge() {
    base::at_active_clk();
  }

  void clk_skew_delay() {
    this->exchange_value(0);
    if (A.get_interface())
      _A = A.read();
    else {
      _is_connected_port_0 = false;
    }
    if (CSN.get_interface())
      _CSN = CSN.read();
    if (D.get_interface())
      _D = D.read();
    else {
      _is_connected_port_0 = false;
    }
    if (WEN.get_interface())
      _WEN = WEN.read();

    //  Write
    int _w_addr_port_0 = -1;
    if ( _is_connected_port_0 && (_CSN==1) && (_WEN==1)) {
      _w_addr_port_0 = get_addr(_A, "A");
      if (_w_addr_port_0 >= 0)
        inject_value(_w_addr_port_0, 0, 128, _D, 0);
    }
    if( !_is_connected_port_0 && !_is_connected_port_0_messaged) {
      std::ostringstream msg;msg << "port_0 is not fully connected and writes on it will be ignored";
      SC_REPORT_WARNING(this->name(), msg.str().c_str());
      _is_connected_port_0_messaged = true;
    }

    //  Sync Read
    if ((_CSN==1)) {
      const int addr = get_addr(_A, "A");
      if (addr >= 0)
      {
        if (addr==_w_addr_port_0) {
          sc_lv<128> dc; // X
          _Q = dc;
        }
        else
          extract_value(addr, 0, 128, _Q, 0);
      }
      else { 
        sc_lv<128> dc; // X
        _Q = dc;
      }
    }
    if (Q.get_interface())
      Q = _Q;
    this->_value_changed.notify(SC_ZERO_TIME);
  }

  int get_addr(const sc_lv<Addr>& addr, const char* pin_name) {
    if (addr.is_01()) {
      const int cur_addr = addr.to_uint();
      if (cur_addr < 0 || cur_addr >= 4096) {
#ifdef CCS_SYSC_DEBUG
        std::ostringstream msg;
        msg << "Invalid address '" << cur_addr << "' out of range [0:" << 4096-1 << "]";
        SC_REPORT_WARNING(pin_name, msg.str().c_str());
#endif
        return -1;
      } else {
        return cur_addr;
      }
    } else {
#ifdef CCS_SYSC_DEBUG
      std::ostringstream msg;
      msg << "Invalid Address '" << addr << "' contains 'X' or 'Z'";
      SC_REPORT_WARNING(pin_name, msg.str().c_str());
#endif
      return -1;
    }
  }

  void reset_memory() {
    this->zero_data();
    _A = sc_lv<Addr>();
    _CSN = SC_LOGIC_X;
    _D = sc_lv<Bits_Func>();
    _WEN = SC_LOGIC_X;
    _is_connected_port_0 = true;
    _is_connected_port_0_messaged = false;
  }

  sc_lv<Addr>  _A;
  sc_logic _CSN;
  sc_lv<Bits_Func>  _D;
  sc_lv<Bits_Func>  _Q;
  sc_logic _WEN;
  bool _is_connected_port_0;
  bool _is_connected_port_0_messaged;
};
#endif // ifndef __INCLUDED_ST_SPHD_BB_4096x128m8_aTdol_wrapper_trans_rsc_H__


