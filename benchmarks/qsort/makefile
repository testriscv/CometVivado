CC=riscv32-unknown-elf-gcc
CCDUMP=riscv32-unknown-elf-objdump
CFLAGS=-Wno-overflow
BUILDDIR=build
RESDIR=res
TYPES = schar uchar short ushort int uint int64 uint64 float
size = 10 50 100 1000

all: qsort.riscv
	
qsort.riscv: qsort.c qsort.h
	mkdir -p $(BUILDDIR) $(RESDIR)
	$(foreach size, $(size), $(foreach type, $(TYPES), $(CC) qsort.c $(CFLAGS) -o $(BUILDDIR)/qsort_$(type)_$(size).riscv -DSIZE=$(size) -DTYPE=$(type);))
	$(foreach size, $(size), $(foreach type, $(TYPES), gcc qsort.c $(CFLAGS) -o $(BUILDDIR)/qsort_$(type)_$(size).x86 -DSIZE=$(size) -DTYPE=$(type);))
	$(CC) qsort.c -o qsort.riscv -D__HLS__ -Wl,-Ttext,4
	$(foreach size, $(size), $(foreach type, $(TYPES), $(BUILDDIR)/qsort_$(type)_$(size).x86 > $(RESDIR)/qsort_$(type)_$(size);))
	$(foreach size, $(size), $(foreach type, $(TYPES), $(CCDUMP) -D $(BUILDDIR)/qsort_$(type)_$(size).riscv > $(BUILDDIR)/qsort_$(type)_$(size).dump;))

clean:
	rm -rf $(BUILDDIR) $(RESDIR) *.riscv *.x86

.PHONY: all clean


