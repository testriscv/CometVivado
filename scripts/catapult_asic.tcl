set WORKING_DIR $::env(PWD)

solution new -state initial
solution options defaults


solution options set /ComponentLibs/TechLibSearchPath /opt/DesignKit/cmos28fdsoi_29/C28SOI_SC_12_CORE_LL/5.1-05/libs

solution options set ComponentLibs/SearchPath /opt/DesignKit/catapult_lib -append
solution options set ComponentLibs/SearchPath /opt/DesignKit/catapult_lib/memory -append

solution options set /Input/CompilerFlags {-D __CATAPULT__ -D __HLS__ -D MEMORY_INTERFACE=IncompleteMemory}
solution options set /Input/SearchPath $WORKING_DIR/../include
solution options set /Output/GenerateCycleNetlist false
solution file add $WORKING_DIR/../src/core.cpp -type C++
directive set -DESIGN_GOAL area

go new
directive set -DESIGN_HIERARCHY doCore
go compile
solution library add C28SOI_SC_12_CORE_LL_ccs -file /opt/DesignKit/catapult_lib/C28SOI_SC_12_CORE_LL_ccs.lib
solution library add ST_singleport_8192x32
go libraries
directive set -CLOCKS {clk {-CLOCK_PERIOD 2 -CLOCK_EDGE rising -CLOCK_UNCERTAINTY 0.0 -CLOCK_HIGH_TIME 1.0 -RESET_SYNC_NAME rst -RESET_ASYNC_NAME arst_n -RESET_KIND sync -RESET_SYNC_ACTIVE high -RESET_ASYNC_ACTIVE low -ENABLE_ACTIVE high}}
go assembly
directive set /doCore/globalStall:rsc -MAP_TO_MODULE {[DirectInput]}
directive set /doCore/core/core.regFile:rsc -MAP_TO_MODULE {[Register]}
directive set /doCore/core/while -PIPELINE_INIT_INTERVAL 1
directive set /doCore/imData:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
directive set /doCore/dmData:rsc -MAP_TO_MODULE ST_singleport_8192x32.ST_SPHD_BB_8192x32m16_aTdol_wrapper
go architect
go extract
