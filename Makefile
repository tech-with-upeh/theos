gcc=/home/codezero/opt/cross/bin/i686-elf-gcc
ld=/home/codezero/opt/cross/bin/i686-elf-ld
CFLAGS = -ffreestanding -g

all: clean kernel boot image

clean:
	rm -rf *.o

kernel:
	$(gcc) $(CFLAGS) -c src/kernel.c -o kernel.o
	$(gcc) $(CFLAGS) -c src/vga.c -o vga.o
	$(gcc) $(CFLAGS) -c src/gdt/gdt.c -o gdt.o
	$(gcc) $(CFLAGS) -c src/util.c -o util.o
	$(gcc) $(CFLAGS) -c src/interrupts/idt.c -o idt.o
	$(gcc) $(CFLAGS) -c src/timer.c -o timer.o
	$(gcc) $(CFLAGS) -c src/stdlib/stdio.c -o stdio.o
	$(gcc) $(CFLAGS) -c src/keyboard.c -o keyboard.o
	$(gcc) $(CFLAGS) -c src/memory.c -o memory.o
	$(gcc) $(CFLAGS) -c src/kmalloc.c -o kmalloc.o
	$(gcc) $(CFLAGS) -c src/task.c -o task.o

boot:
	nasm -f elf32 src/boot.s -o boot.o
	nasm -f elf32 src/gdt/gdt.s -o gdts.o
	nasm -f elf32 src/interrupts/idt.s -o idts.o
image:
	$(ld) -T linker.ld -o kernel boot.o kernel.o vga.o gdt.o gdts.o util.o idt.o idts.o timer.o stdio.o keyboard.o memory.o kmalloc.o task.o
	mv kernel Jazz/boot/kernel
	grub-mkrescue -o kernel.iso Jazz/
	rm *.o
qm:
	qemu-system-i386 -drive format=raw,file=kernel.iso


dbg:
	qemu-system-i386 -s -S -drive format=raw,file=kernel.iso

qml:
	qemu-system-i386 -drive format=raw,file=kernel.iso -d int,cpu_reset -no-reboot -no-shutdown -D qemu.log