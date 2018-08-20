CC = riscv32-unknown-elf-gcc
CCFLAGS=-Wno-overflow 
BUILDDIR = build
RESDIR = res
benchmarks = matmul qsort dijkstra

all:  
	mkdir -p $(BUILDDIR) $(RESDIR)
	$(foreach bench, $(benchmarks), $(MAKE) -C $(bench); cp $(bench)/build/* build/; cp $(bench)/res/* res/;)

clean:
	$(foreach bench, $(benchmarks), $(MAKE) -C $(bench) clean;)

mrproper: clean
	rm -rf $(BUILDDIR) $(RESDIR)

.PHONY: all clean mrproper
