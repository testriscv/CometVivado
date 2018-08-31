CFLAGS=
all: riscv x86

riscv:
	riscv32-unknown-elf-gcc test_syscalls.c -o syscalls.riscv $(CFLAGS)
	riscv32-unknown-elf-objdump -D syscalls.riscv > syscalls.dump

x86:
	gcc test_syscalls.c -o syscalls.x86 $(CFLAGS)

clean:
	rm syscalls.*
