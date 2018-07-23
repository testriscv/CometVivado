AC=./include/catapult
INC=$(AC) ./include
LINC=./include
INC_PARAMS=$(foreach d, $(INC), -I$d)
VARS_CAT=-D__CATAPULT__=1
VARS_VIV=-D__VIVADO__=1
DEFINES=
FILES=cache.cpp core.cpp elfFile.cpp portability.cpp reformeddm_sim.cpp simulator.cpp 
S=./src
S_FILES=$(foreach f, $(FILES), $(S)/$f)
EADER=cache.h core.h elf.h elfFile.h portability.h riscvISA.h simulator.h
I_HEADER=$(foreach f, $(HEADER), $(INC)/$f)

all: $(S_FILES) $(I_HEADER)
	g++ -O3 -o comet.sim $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES)

catapult: $(S_FILES) $(I_HEADER)
	g++ -o comet.sim $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES) -D__SYNTHESIS__

debugcatapult: $(S_FILES) $(I_HEADER)
	g++ -g -o comet.sim $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES) -D__SYNTHESIS__ -D__DEBUG__

text: $(S_FILES) $(I_HEADER)
	g++ -E $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES) -D__DEBUG__ > comet.cpp

textcatapult: $(S_FILES) $(I_HEADER)
	g++ -E $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES) -D__SYNTHESIS__ > comet.cpp

debug: $(S_FILES) $(I_HEADER)
	g++ -g -o comet.sim $(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES) -D__DEBUG__ 

vivado.sim: $(S_FILES) $(I_HEADER) 
	g++ -o vivado.sim $(INC_PARAMS) $(S_FILES) $(VARS_VIV)

clean:
	rm -rf *.o comet.sim vivado.sim comet.cpp 

.PHONY: all catapult clean debug text textcatapult

