objects = matmul4_4.o matmul8_8.o matmul16_16.o matmul32_32.o
executables = $(OUT_DIR)/matmul4_4.out $(OUT_DIR)/matmul8_8.out $(OUT_DIR)/matmul16_16.out $(OUT_DIR)/matmul32_32.out $(OUT_DIR)/qsort_10.out $(OUT_DIR)/qsort_50.out $(OUT_DIR)/qsort_100.out $(OUT_DIR)/qsort_1000.out
dependencies = src/matmul.c include/matmul.h src/qsort.c include/qsort.h
CCX = riscv32-unknown-elf-gcc
FLAGS=-Wno-overflow 
INCLUDE = ./include
OUT_DIR = ./build
TYPES = schar uchar short ushort int uint int64 uint64 float 

benchmarks = matmul qsort
msize = 4 8 16 32
qsize = 10 50 100 1000

all: $(dependencies)
	mkdir -p build res
	$(foreach size, $(msize), $(foreach type, $(TYPES), $(CCX) -I $(INCLUDE) src/matmul.c $(FLAGS) -o $(OUT_DIR)/matmul_$(type)_$(size).riscv -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), $(CCX) -I $(INCLUDE) src/qsort.c $(FLAGS) -o $(OUT_DIR)/qsort_$(type)_$(size).riscv -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(msize), $(foreach type, $(TYPES), gcc -I $(INCLUDE) src/matmul.c $(FLAGS) -o build/matmul_$(type)_$(size) -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), gcc -I $(INCLUDE) src/qsort.c $(FLAGS) -o build/qsort_$(type)_$(size) -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(msize), $(foreach type, $(TYPES), build/matmul_$(type)_$(size) > res/matmul_$(type)_$(size);))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), build/qsort_$(type)_$(size) > res/qsort_$(type)_$(size);))
	$(foreach size, $(msize), $(foreach type, $(TYPES), riscv32-unknown-elf-objdump -D build/matmul_$(type)_$(size).riscv > build/matmul_$(type)_$(size).txt;))
	$(foreach size, $(qsize), $(foreach type, $(TYPES), riscv32-unknown-elf-objdump -D build/qsort_$(type)_$(size).riscv > build/qsort_$(type)_$(size).txt;))
	$(CCX) -I $(INCLUDE) src/matmul.c $(FLAGS) -D__HLS__ -o $(OUT_DIR)/matmul.riscv -Wl,-Ttext,4
	riscv32-unknown-elf-objdump -D build/matmul.riscv > build/matmul.txt	

clean:
	rm -rf build res

.PHONY: all clean directories test
