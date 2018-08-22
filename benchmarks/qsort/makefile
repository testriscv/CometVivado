XLEN?=32
CC=riscv$(XLEN)-unknown-elf-gcc
CCDUMP=riscv$(XLEN)-unknown-elf-objdump
CFLAGS=-Wno-overflow
BUILDDIR=build
RESDIR=res
TYPES = schar uchar short ushort int uint int64 uint64 float
size = 10 50 100 1000

all: qsort.riscv$(XLEN)
	
qsort.riscv$(XLEN): qsort.c qsort.h
	mkdir -p $(BUILDDIR) $(RESDIR)
	$(foreach size, $(size), $(foreach type, $(TYPES), $(CC) qsort.c $(CFLAGS) -o $(BUILDDIR)/qsort_$(type)_$(size).riscv$(XLEN) -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(size), $(foreach type, $(TYPES), gcc qsort.c $(CFLAGS) -o $(BUILDDIR)/qsort_$(type)_$(size).x86 -DSIZE=$(size) -DTYPE=$(type);))
	$(CC) qsort.c -o qsort.riscv$(XLEN) -D__HLS__ -Wl,-Ttext,4
	$(foreach size, $(size), $(foreach type, $(TYPES), $(BUILDDIR)/qsort_$(type)_$(size).x86 > $(RESDIR)/qsort_$(type)_$(size);))
	$(foreach size, $(size), $(foreach type, $(TYPES), $(CCDUMP) -D $(BUILDDIR)/qsort_$(type)_$(size).riscv$(XLEN) > $(BUILDDIR)/qsort_$(type)_$(size).dump;))

clean:
	rm -rf $(BUILDDIR) $(RESDIR) *.riscv32 *.riscv64 *.x86

.PHONY: all clean


