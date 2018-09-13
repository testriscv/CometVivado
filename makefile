AC=./include/catapult
INC=$(AC) ./include
LINC=./include
INC_PARAMS=$(foreach d, $(INC), -I$d)
VARS_CAT=-D__CATAPULT__=1
VARS_VIV=-D__VIVADO__=1
DEFINES=
FILES=cache.cpp core.cpp elfFile.cpp multicycleoperator.cpp portability.cpp reformeddm_sim.cpp simulator.cpp 
S=./src
S_FILES=$(foreach f, $(FILES), $(S)/$f)
HEADER=cache.h core.h elf.h elfFile.h multicycleoperator.h portability.h riscvISA.h simulator.h
I_HEADER=$(foreach f, $(HEADER), $(INC)/$f)
GENERIC=$(INC_PARAMS) $(S_FILES) $(VARS_CAT) $(DEFINES) -std=c++98

all: $(S_FILES) $(I_HEADER)
	g++ -O3 -o comet.sim $(GENERIC)

dall: $(S_FILES) $(I_HEADER)
	g++ -g -o comet.sim $(GENERIC)

catapult: $(S_FILES) $(I_HEADER)
	g++ -o comet.sim $(GENERIC) -D__SYNTHESIS__

debugcatapult: $(S_FILES) $(I_HEADER)
	g++ -g -o comet.sim $(GENERIC) -D__SYNTHESIS__ -D__DEBUG__

sanitize: $(S_FILES) $(I_HEADER)
	g++ -g -o comet.sim $(GENERIC) -fsanitize=address -fsanitize=undefined -fsanitize=shift -fsanitize=shift-exponent -fsanitize=shift-base -fsanitize=integer-divide-by-zero -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=null -fsanitize=return -fsanitize=signed-integer-overflow -fsanitize=bounds -fsanitize=bounds-strict -fsanitize=bool -fsanitize=enum 

text: $(S_FILES) $(I_HEADER)
	g++ -E $(GENERIC) -D__DEBUG__ > comet.cpp

textcatapult: $(S_FILES) $(I_HEADER)
	g++ -E $(GENERIC) -D__SYNTHESIS__ > catapult.cpp

debug: $(S_FILES) $(I_HEADER)
	g++ -g -o comet.sim $(GENERIC) -D__DEBUG__ 

vivado.sim: $(S_FILES) $(I_HEADER) 
	g++ -o vivado.sim $(INC_PARAMS) $(S_FILES) $(VARS_VIV)

clean:
	rm -rf *.o comet.sim vivado.sim comet.cpp catapult.cpp 

.PHONY: all catapult clean debug text textcatapult

