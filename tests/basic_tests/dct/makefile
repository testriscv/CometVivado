XLEN?=32
CCRV=riscv$(XLEN)-unknown-elf-gcc
CCHOST=gcc
CFLAGS=-Wno-overflow
EXEC=dct

all: $(EXEC).riscv$(XLEN)

$(EXEC).riscv$(XLEN): $(EXEC).c
	$(CCRV) $(EXEC).c $(CFLAGS) -o $(EXEC).riscv$(XLEN)
	$(CCHOST) $(EXEC).c -o $(EXEC)
	./$(EXEC) > expectedOutput
	rm -f $(EXEC)

clean:
	rm -f *.riscv* expectedOutput
