gcc=/home/codezero/opt/cross/bin/i686-elf-gcc
ld=/home/codezero/opt/cross/bin/i686-elf-ld
CFLAGS = -ffreestanding -g -Isrc

# 1. Cleanly grab all source files, removing any leading "./" paths
C_SOURCES := $(patsubst ./%, %, $(shell find src -name "*.c"))
ASM_SOURCES := $(patsubst ./%, %, $(shell find src -name "*.s"))

# 2. Separate boot.s so it is explicitly forced to be linked first
BOOT_SOURCE := src/boot.s
BOOT_OBJECT := obj/src/boot.o

# Filter out boot.s from the rest of the assembly files
OTHER_ASM_SOURCES := $(filter-out $(BOOT_SOURCE), $(ASM_SOURCES))

# --- CRITICAL FIX: Append .asm.o to assembly files to prevent collisions with .c files! ---
# e.g., src/gdt/gdt.c -> obj/src/gdt/gdt.o
# e.g., src/gdt/gdt.s -> obj/src/gdt/gdt.asm.o
C_OBJECTS := $(patsubst %.c, obj/%.o, $(C_SOURCES))
OTHER_ASM_OBJECTS := $(patsubst %.s, obj/%.asm.o, $(OTHER_ASM_SOURCES))

all: clean initrd image

clean:
	rm -rf obj kernel.iso Jazz/boot/initrd.tar

# Rule to compile C files into standard .o targets
obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(gcc) $(CFLAGS) -c $< -o $@

# --- MATCHING FIX: Pattern rule to assemble raw .s into distinct .asm.o targets ---
obj/%.asm.o: %.s
	@mkdir -p $(dir $@)
	nasm -f elf32 $< -o $@

# Explicit separate rule for boot.o since it acts as our static head anchor
$(BOOT_OBJECT): $(BOOT_SOURCE)
	@mkdir -p $(dir $@)
	nasm -f elf32 $< -o $@

initrd:
	mkdir -p initrd_root
	touch initrd_root/test.txt
	echo "Hello from GRUB Ramdisk!" > initrd_root/test.txt
	cd initrd_root && tar -cf ../Jazz/boot/initrd.tar *

image: $(BOOT_OBJECT) $(C_OBJECTS) $(OTHER_ASM_OBJECTS)
	@echo "Linking files cleanly without naming collisions!"
	$(ld) -T linker.ld -o kernel $(BOOT_OBJECT) $(C_OBJECTS) $(OTHER_ASM_OBJECTS)
	mv kernel Jazz/boot/kernel
	grub-mkrescue -o kernel.iso Jazz/

qm:
	qemu-system-i386 -drive format=raw,file=kernel.iso

dbg:
	qemu-system-i386 -s -S -drive format=raw,file=kernel.iso

qml:
	qemu-system-i386 -drive format=raw,file=kernel.iso -d int,cpu_reset -no-reboot -no-shutdown -D qemu.log
