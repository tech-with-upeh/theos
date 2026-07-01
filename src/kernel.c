#include "vga.h"
#include "stdint.h"
#include "gdt/gdt.h"
#include "interrupts/idt.h"
#include "timer.h"
#include "kmalloc.h"
#include "stdlib/stdio.h"
#include "keyboard.h"
#include "multiboot.h"
#include "memory.h"
#include "util.h"
#include "task.h"
#include "vfs.h"
#include "initrd.h"
#include "pci.h"
#include "ata.h"

static inline void asm_enable_interrupts() {
    __asm__ volatile("sti");
}
void kmain(uint32_t magic, struct multiboot_info* bootInfo);

void kmain(uint32_t magic, struct multiboot_info* bootInfo){
    initGdt();
    print("GDT is done!\r\n");
    initIdt();
    initTimer();
    
    // 1. Read baseline parameters safely from bootInfo
    uint32_t mem_upper      = bootInfo->mem_upper;
    uint32_t flags          = bootInfo->flags;
    uint32_t mods_count     = bootInfo->mods_count;
    uint32_t mods_addr      = bootInfo->mods_addr;

    // 2. CRITICAL PRE-EXTRACT: Grab the module boundaries right now!
    uint32_t tar_start = 0;
    uint32_t tar_end = 0;

    // Ensure the multiboot flag bit 3 is set (indicates modules are present)
    if ((flags & (1 << 3)) && mods_count > 0) {
        // Cast the physical address to a readable pointer (valid during early boot identity mapping)
        multiboot_module_t* mod_array = (multiboot_module_t*)mods_addr;
        tar_start = mod_array[0].mod_start;
        tar_end = mod_array[0].mod_end;
    }

    printf("Early Multiboot Module Check: start=%x end=%x\n", tar_start, tar_end);

    uint32_t physicalAllocStart = 0x00200000;

    // NOW it's safe to tear down the lower memory mappings
    initMemory(mem_upper * 1024, physicalAllocStart);

    printf("physicalAllocStart=%x\n", physicalAllocStart);
    printf("mem_upper_bytes=%x\n", mem_upper * 1024);

    kmallocInit(0x1000);
    init_multitasking();
    print("Memory allocation done!\n");

    pci_scan_bus();
    init_ata(); 

    vfs_node_t* hd = ata_get_vfs_node();

    // 1. Prepare data chunks
    uint8_t write_payload[13] = "OSDev Rocks!";
    uint8_t read_verify[13] = {0}; // +1 for null terminator

    // 2. Commit payload to offset address 0
    vfs_write(hd, 0, 12, write_payload);
    printf("Disk write committed!\n");

    // 3. Clear data and read it back from disk to verify
    vfs_read(hd, 0, 13, read_verify);
    printf("Disk read verification value: %s\n", (char*)read_verify);
    // --- PASS THE HIGHLY ACCURATE PHYSICAL EXTRACTS ---
    initrd_init(tar_start, tar_end);

    // spawn_user_task(&my_userspace_task);
    // print("Userspace task registered.\n");

    // spawn_program("shell.bin");
    asm_enable_interrupts();
    initKeyboard();
    printf("Kernel initialization complete. Entering idle loop...\n");
    for(;;);
}
