set WORKING_DIR $::env(PWD)

solution new -state initial
solution options defaults


solution options set /ComponentLibs/TechLibSearchPath /opt/DesignKit/cmos28fdsoi_29/C28SOI_SC_12_CORE_LL/5.1-05/libs
solution options set /ComponentLibs/SearchPath /opt/Catapult-10.0b/Mgc_home/pkgs/siflibs/designcompiler -append
solution options set /Input/CompilerFlags {-D __CATAPULT__ -D __HLS__ -D MEMORY_INTERFACE=IncompleteMemory}
solution options set /Input/SearchPath $WORKING_DIR/../include
solution options set /Output/GenerateCycleNetlist false
solution file add $WORKING_DIR/../src/core.cpp -type C++

directive set -DESIGN_GOAL area

go new
directive set -DESIGN_HIERARCHY doCore
go compile
solution library add mgc_Xilinx-ZYNQ-uplus-2L_beh -- -rtlsyntool Vivado -manufacturer Xilinx -family ZYNQ-uplus -speed -2L -part xczu11eg-ffvb1517-2L-e
solution library add Xilinx_RAMS
solution library add Xilinx_ROMS
solution library add amba
solution library add Xilinx_FIFO
go libraries
directive set -CLOCKS {clk {-CLOCK_PERIOD 10.0 -CLOCK_EDGE rising -CLOCK_UNCERTAINTY 0.0 -CLOCK_HIGH_TIME 5.0 -RESET_SYNC_NAME rst -RESET_ASYNC_NAME arst_n -RESET_KIND sync -RESET_SYNC_ACTIVE high -RESET_ASYNC_ACTIVE low -ENABLE_ACTIVE high}}
go assembly
directive set /doCore/globalStall:rsc -MAP_TO_MODULE {[DirectInput]}
directive set /doCore/imData:rsc -MAP_TO_MODULE Xilinx_RAMS.BLOCK_SPRAM_NOCHANGE
directive set /doCore/dmData:rsc -MAP_TO_MODULE Xilinx_RAMS.BLOCK_SPRAM_NOCHANGE
directive set /doCore/core/core.regFile:rsc -MAP_TO_MODULE {[Register]}
directive set /doCore/core/while -PIPELINE_INIT_INTERVAL 1
directive set /doCore/core/core.multiplicationUnit.process:if:else:for -UNROLL yes
go architect
go extract
