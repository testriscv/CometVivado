AC=./include/catapult
INC=$(AC) ./include
LINC=./include
INC_PARAMS=$(foreach d, $(INC), -I$d)
VARS_CAT=-D__DEBUG__=1 -D__CATAPULT__=1
VARS_VIV=-D__DEBUG__=1 -D__VIVADO__=1
DEFINES=
FILES=cache.cpp core.cpp elfFile.cpp portability.cpp reformeddm_sim.cpp simulator.cpp 
S=./src
S_FILES=$(foreach f, $(FILES), $(S)/$f)
EADER=cache.h core.h elf.h elfFile.h portability.h riscvISA.h simulator.h
I_HEADER=$(foreach f, $(HEADER), $(INC)/$f)

catapult: $(S_FILES) $(I_HEADER)
	g++ -O3 -o comet.sim $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES)

debug: $(S_FILES) $(I_HEADER)
	g++ -g -o comet.sim $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES) 

vivado.sim: $(S_FILES) $(I_HEADER) 
	g++ -o vivado.sim $(INC_PARAMS) $(S_FILES) $(VARS_VIV)

clean:
	rm -rf *.o catapult.sim vivado.sim 

.PHONY: catapult clean debug

