flow package require MemGen
flow run /MemGen/MemoryGenerator_BuildLib {
VENDOR           STMicroelectronics
RTLTOOL          DesignCompiler
TECHNOLOGY       {28nm FDSOI}
LIBRARY          ST_singleport_4096x128
MODULE           ST_SPHD_BB_4096x128m8_aTdol_wrapper
OUTPUT_DIR       /udd/vegloff/Documents/comet/memories/
FILES {
  { FILENAME /opt/DesignKit/cmos28fdsoi_29/memcut_28nm/C28SOI_SPHD_BB_170612/4.3-00.00/behaviour/verilog/SPHD_BB_170612.v         FILETYPE Verilog MODELTYPE generic PARSE 1 PATHTYPE abs STATICFILE 1 VHDL_LIB_MAPS work }
  { FILENAME /opt/DesignKit/cmos28fdsoi_29/memcut_28nm/C28SOI_SPHD_BB_170612/4.3-00.00/behaviour/verilog/SPHD_BB_170612_wrapper.v FILETYPE Verilog MODELTYPE generic PARSE 1 PATHTYPE abs STATICFILE 1 VHDL_LIB_MAPS work }
}
VHDLARRAYPATH    {}
WRITEDELAY       0.194
INITDELAY        1
READDELAY        0.940
VERILOGARRAYPATH {}
INPUTDELAY       0.146
WIDTH            128
AREA             53000
RDWRRESOLUTION   UNKNOWN
WRITELATENCY     1
READLATENCY      1
DEPTH            4096
PARAMETERS {
  { PARAMETER Words                TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER Bits                 TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER mux                  TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER Bits_Func            TYPE hdl IGNORE 0 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER mask_bits            TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER repair_address_width TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER Addr                 TYPE hdl IGNORE 0 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER read_margin_size     TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
  { PARAMETER write_margin_size    TYPE hdl IGNORE 1 MIN {} MAX {} DEFAULT 0 }
}
PORTS {
  { NAME port_0 MODE ReadWrite }
}
PINMAPS {
  { PHYPIN A     LOGPIN ADDRESS      DIRECTION in  WIDTH Addr      PHASE {} DEFAULT {} PORTS port_0 }
  { PHYPIN CK    LOGPIN CLOCK        DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {} PORTS port_0 }
  { PHYPIN CSN   LOGPIN CHIP_SELECT  DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {} PORTS {}     }
  { PHYPIN D     LOGPIN DATA_IN      DIRECTION in  WIDTH Bits_Func PHASE {} DEFAULT {} PORTS port_0 }
  { PHYPIN INITN LOGPIN UNCONNECTED  DIRECTION in  WIDTH 1.0       PHASE {} DEFAULT 1  PORTS {}     }
  { PHYPIN Q     LOGPIN DATA_OUT     DIRECTION out WIDTH Bits_Func PHASE {} DEFAULT {} PORTS port_0 }
  { PHYPIN WEN   LOGPIN WRITE_ENABLE DIRECTION in  WIDTH 1.0       PHASE 1  DEFAULT {} PORTS port_0 }
}

}
