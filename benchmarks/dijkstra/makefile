XLEN ?= 32
CC=riscv$(XLEN)-unknown-elf-gcc
CCDUMP=riscv$(XLEN)-unknown-elf-objdump
CFLAGS=-Wno-overflow
BUILDDIR=build
RESDIR=res
TYPES = schar uchar short ushort int uint int64 uint64 float
size = 10 100 # 300 takes around 6 hours...

all: dijkstra.riscv$(XLEN)
	
dijkstra.riscv$(XLEN): dijkstra.c dijkstra.h
	mkdir -p $(BUILDDIR) $(RESDIR)
	$(foreach size, $(size), $(CC) dijkstra.c $(CFLAGS) -o $(BUILDDIR)/dijkstra_$(size).riscv$(XLEN) -DSIZE=$(size);)
	$(foreach size, $(size), gcc dijkstra.c $(CFLAGS) -o $(BUILDDIR)/dijkstra_$(size).x86 -DSIZE=$(size);)
	$(CC) dijkstra.c -o dijkstra.riscv$(XLEN) -D__HLS__ -Wl,-Ttext,4
	$(foreach size, $(size), $(BUILDDIR)/dijkstra_$(size).x86 > $(RESDIR)/dijkstra_$(size);)
	$(foreach size, $(size), $(CCDUMP) -D $(BUILDDIR)/dijkstra_$(size).riscv$(XLEN) > $(BUILDDIR)/dijkstra_$(size).dump;)

clean:
	rm -rf $(BUILDDIR) $(RESDIR) *.riscv32 *.riscv64 *.x86

.PHONY: all clean


