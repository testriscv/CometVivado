XLEN?=32
CC=riscv$(XLEN)-unknown-elf-gcc
CCDUMP=riscv$(XLEN)-unknown-elf-objdump
CFLAGS=-Wno-overflow
BUILDDIR=build
RESDIR=res
TYPES = schar uchar short ushort int uint int64 uint64 float
size = 4 8 16 32 

all: matmul.riscv$(XLEN)
	
matmul.riscv$(XLEN): matmul.c matmul.h
	mkdir -p $(BUILDDIR) $(RESDIR)
	$(foreach size, $(size), $(foreach type, $(TYPES), $(CC) matmul.c $(CFLAGS) -o $(BUILDDIR)/matmul_$(type)_$(size).riscv$(XLEN) -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(size), $(foreach type, $(TYPES), gcc matmul.c $(CFLAGS) -o $(BUILDDIR)/matmul_$(type)_$(size).x86 -DSIZE=$(size) -DTYPE=$(type);))
	$(CC) matmul.c -o $(BUILDDIR)/matmul.riscv$(XLEN) -D__HLS__ -Wl,-Ttext,4
	$(CC) matmul.c -o matmul.riscv$(XLEN) -D__HLS__ -Wl,-Ttext,4
	$(foreach size, $(size), $(foreach type, $(TYPES), $(BUILDDIR)/matmul_$(type)_$(size).x86 > $(RESDIR)/matmul_$(type)_$(size);))
	$(foreach size, $(size), $(foreach type, $(TYPES), $(CCDUMP) -D $(BUILDDIR)/matmul_$(type)_$(size).riscv$(XLEN) > $(BUILDDIR)/matmul_$(type)_$(size).dump;))

clean:
	rm -rf $(BUILDDIR) $(RESDIR) *.riscv32 *.riscv64 *.x86

.PHONY: all clean


