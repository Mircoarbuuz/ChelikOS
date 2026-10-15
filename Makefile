CC = x86_64-elf-gcc
LD = x86_64-elf-ld
AS = nasm

CFLAGS = -ffreestanding -O2 -Wall -Wextra -I.
ASFLAGS = -f elf64

ASM_SOURCES = boot.asm libc/drivers/switch.asm libc/drivers/isr.asm
C_SOURCES = \
    kernel.c \
    libc/drivers/process.c \
    libc/drivers/pit.c \
    libc/drivers/disk.c \
    libc/fs/fat.c \
    libc/string.c

ASM_OBJS = $(ASM_SOURCES:.asm=.o)
C_OBJS = $(C_SOURCES:.c=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

all: chelikos.iso

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS) linker.ld
	$(LD) -n -T linker.ld -o kernel.bin $(OBJS)

chelikos.iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	if [ ! -f grub.cfg ]; then \
		echo "set timeout=0" > iso/boot/grub/grub.cfg; \
		echo "set default=0" >> iso/boot/grub/grub.cfg; \
		echo "insmod vbe" >> iso/boot/grub/grub.cfg; \
		echo "insmod vga" >> iso/boot/grub/grub.cfg; \
		echo "set gfxpayload=\"1024x768x16\"" >> iso/boot/grub/grub.cfg; \
		echo "" >> iso/boot/grub/grub.cfg; \
		echo "menuentry \"ChelikOS\" {" >> iso/boot/grub/grub.cfg; \
		echo "    multiboot2 /boot/kernel.bin" >> iso/boot/grub/grub.cfg; \
		echo "    boot" >> iso/boot/grub/grub.cfg; \
		echo "}" >> iso/boot/grub/grub.cfg; \
	else \
		cp grub.cfg iso/boot/grub/grub.cfg; \
	fi
	grub-mkrescue -o chelikos.iso iso
dev: chelikos.iso
	qemu-system-x86_64 -hda fat32.img -cdrom chelikos.iso -m 512M -boot d
clean:
	rm -rf *.o libc/drivers/*.o libc/fs/*.o libc/*.o kernel.bin chelikos.iso iso