# Toolchain definition
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
AS = nasm

# Compiler and assembler flags
CFLAGS = -ffreestanding -O2 -Wall -Wextra -I.
ASFLAGS = -f elf64

# Source files
ASM_SOURCES = boot.asm
C_SOURCES = \
    kernel.c \
    libc/drivers/process.c \
    libc/drivers/disk.c \
    libc/fs/fat.c \
    libc/string.c

# Object files
ASM_OBJS = $(ASM_SOURCES:.asm=.o)
C_OBJS = $(C_SOURCES:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

# Default target
all: chelikos.iso

# Assemble bootloader/startup code
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Compile C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link kernel object files using linker script
kernel.bin: $(OBJS) linker.ld
	$(LD) -n -T linker.ld -o kernel.bin $(OBJS)

# Build the bootable ISO using GRUB (and generate grub.cfg automatically)
chelikos.iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	if [ ! -f grub.cfg ]; then \
		echo "set timeout=0" > iso/boot/grub/grub.cfg; \
		echo "set default=0" >> iso/boot/grub/grub.cfg; \
		echo "" >> iso/boot/grub/grub.cfg; \
		echo "menuentry \"ChelikOS\" {" >> iso/boot/grub/grub.cfg; \
		echo "    multiboot2 /boot/kernel.bin" >> iso/boot/grub/grub.cfg; \
		echo "    boot" >> iso/boot/grub/grub.cfg; \
		echo "}" >> iso/boot/grub/grub.cfg; \
	else \
		cp grub.cfg iso/boot/grub/grub.cfg; \
	fi
	grub-mkrescue -o chelikos.iso iso

# Convenience target to test in QEMU
dev: chelikos.iso
	qemu-system-x86_64 -hda fat32.img -cdrom chelikos.iso -m 512M -boot d

# Clean up build artifacts
clean:
	rm -rf *.o libc/**/*.o libc/*.o kernel.bin chelikos.iso iso