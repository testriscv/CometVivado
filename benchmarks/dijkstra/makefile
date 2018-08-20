CC=riscv32-unknown-elf-gcc
CCDUMP=riscv32-unknown-elf-objdump
CFLAGS=-Wno-overflow
BUILDDIR=build
RESDIR=res
TYPES = schar uchar short ushort int uint int64 uint64 float
size = 10 100 300

all: dijkstra.riscv
	
dijkstra.riscv: dijkstra.c dijkstra.h
	mkdir -p $(BUILDDIR) $(RESDIR)
	$(foreach size, $(size), $(CC) dijkstra.c $(CFLAGS) -o $(BUILDDIR)/dijkstra_$(size).riscv -DSIZE=$(size);)
	$(foreach size, $(size), gcc dijkstra.c $(CFLAGS) -o $(BUILDDIR)/dijkstra_$(size).x86 -DSIZE=$(size);)
	$(CC) dijkstra.c -o dijkstra.riscv -D__HLS__ -Wl,-Ttext,4
	$(foreach size, $(size), $(BUILDDIR)/dijkstra_$(size).x86 > $(RESDIR)/dijkstra_$(size);)
	$(foreach size, $(size), $(CCDUMP) -D $(BUILDDIR)/dijkstra_$(size).riscv > $(BUILDDIR)/dijkstra_$(size).dump;)

clean:
	rm -rf $(BUILDDIR) $(RESDIR) *.riscv *.x86

.PHONY: all clean


