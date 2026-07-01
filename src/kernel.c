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
#include "customfs.h"

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
    // 1. Scan the PCI Bus FIRST to register the driver with the correct channel
        // 1. Scan the PCI Bus to map hardware drivers
    pci_scan_bus();

    // 2. Load custom filesystem operations onto storage media
    if (ata_get_vfs_node() != NULL) {
        init_customfs();

        // Check current drive directory table states right out of the gate
        customfs_list_files();

        // Create trial sample files AND standard folders into index configurations
        customfs_create_file("notes.txt", 1);
        customfs_create_dir("bin");
        customfs_create_dir("documents");

        // Print directory mapping output indexes again to verify they are registered!
        customfs_list_files();
    } else {
        printf("Hardware Warning: Cannot initialize CustomFS. ATA device missing.\n");
    }



    printf("Kernel initialization complete. Entering idle loop...\n");

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
