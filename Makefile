ASM = nasm
CC = x86_64-elf-gcc
LD = x86_64-elf-ld

ASMFLAGS = -f elf64
CFLAGS = -m64 -ffreestanding -fno-pie -c 
LDFLAGS = -m elf_x86_64 -T linker.ld

all: os.iso

boot.o: boot.asm
	$(ASM) $(ASMFLAGS) boot.asm -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) kernel.c -o kernel.o

libc/fs/fat.o: libc/fs/fat.c
	$(CC) $(CFLAGS) libc/fs/fat.c -o libc/fs/fat.o

libc/drivers/disk.o: libc/drivers/disk.c
	$(CC) $(CFLAGS) libc/drivers/disk.c -o libc/drivers/disk.o

libc/string.o: libc/string.c
	$(CC) $(CFLAGS) libc/string.c -o libc/string.o

kernel.bin: boot.o libc/drivers/disk.o libc/fs/fat.o libc/string.o kernel.o linker.ld
	$(LD) $(LDFLAGS) boot.o libc/drivers/disk.o libc/fs/fat.o libc/string.o kernel.o -o kernel.bin

iso/boot/grub/grub.cfg:
	mkdir -p iso/boot/grub
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "ChelikOS" {' >> iso/boot/grub/grub.cfg
	echo '    multiboot2 /boot/kernel.bin' >> iso/boot/grub/grub.cfg
	echo '    boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg

os.iso: kernel.bin iso/boot/grub/grub.cfg
	mkdir -p iso/boot
	cp kernel.bin iso/boot/kernel.bin
	grub-mkrescue -o chelik.iso iso

run: os.iso
	qemu-system-x86_64 -cdrom chelik.iso -boot d -hda fat32.img

clean:
	rm -rf *.o libc/*.o libc/fs/*.o libc/drivers/*.o kernel.bin *.iso iso

dev: clean run