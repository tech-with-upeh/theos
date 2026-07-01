gcc=/home/codezero/opt/cross/bin/i686-elf-gcc
ld=/home/codezero/opt/cross/bin/i686-elf-ld
CFLAGS = -ffreestanding -g -Isrc

C_SOURCES := $(patsubst ./%, %, $(shell find src -name "*.c" -not -path "src/apps/*"))
ASM_SOURCES := $(patsubst ./%, %, $(shell find src -name "*.s"))

BOOT_SOURCE := src/boot.s
BOOT_OBJECT := obj/src/boot.o

OTHER_ASM_SOURCES := $(filter-out $(BOOT_SOURCE), $(ASM_SOURCES))
C_OBJECTS := $(patsubst %.c, obj/%.o, $(C_SOURCES))
OTHER_ASM_OBJECTS := $(patsubst %.s, obj/%.asm.o, $(OTHER_ASM_SOURCES))

all: clean user_apps initrd image

clean:
	rm -rf obj kernel.iso Jazz/boot/initrd.tar initrd_root

obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(gcc) $(CFLAGS) -c $< -o $@

obj/%.asm.o: %.s
	@mkdir -p $(dir $@)
	nasm -f elf32 $< -o $@

$(BOOT_OBJECT): $(BOOT_SOURCE)
	@mkdir -p $(dir $@)
	nasm -f elf32 $< -o $@

# --- NEW: Compile your independent user applications completely separate from the kernel ---
user_apps:
	@mkdir -p initrd_root
	@mkdir -p obj
	# FIX: Added -fno-pic to guarantee linear local layout offsets
	$(gcc) $(CFLAGS) -fno-pic -fno-stack-protector -ffunction-sections -c src/apps/libc/ulib.c -o obj/ulib.o
	$(gcc) $(CFLAGS) -fno-pic -fno-stack-protector -ffunction-sections -c src/apps/shell.c -o obj/shell.o
	$(ld) -T src/apps/userlinker.ld --oformat binary obj/ulib.o obj/shell.o -o initrd_root/shell.bin


initrd:
	# Pack the generated binaries directly into the multiboot archive
	touch initrd_root/test.txt
	echo "Hello from GRUB Ramdisk!" > initrd_root/test.txt
	cd initrd_root && tar -cf ../Jazz/boot/initrd.tar *

image: $(BOOT_OBJECT) $(C_OBJECTS) $(OTHER_ASM_OBJECTS)
	$(ld) -T linker.ld -o kernel $(BOOT_OBJECT) $(C_OBJECTS) $(OTHER_ASM_OBJECTS)
	mv kernel Jazz/boot/kernel
	grub-mkrescue -o kernel.iso Jazz/

qm:
	qemu-system-i386 -drive format=raw,file=kernel.iso

qmss:
	dd if=/dev/zero of=disk.img bs=1M count=10 status=none
	qemu-system-i386 -boot d -cdrom kernel.iso -drive format=raw,file=disk.img,if=ide,index=0,media=disk

dbg:
	qemu-system-i386 -s -S -drive format=raw,file=kernel.iso

qml:
	qemu-system-i386 -drive format=raw,file=kernel.iso -d int,cpu_reset -no-reboot -no-shutdown -D qemu.log
