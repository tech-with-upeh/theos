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

#include "libc/ulib.h"

static inline void asm_enable_interrupts() {
    __asm__ volatile("sti");
}

// "__attribute__((naked))" forces GCC to omit function prologues and epilogues.
// This completely stops stack frame register pollution during raw code copying!
void __attribute__((naked)) my_userspace_task() {
    __asm__ volatile (
        "jmp .user_init\n\t"
        
        // Embed the filename and a clean buffer directly within our task memory pool
        ".filename_str:\n\t"
        ".ascii \"test.txt\\0\"\n\t"
        
        ".align 4\n\t"
        ".user_buffer:\n\t"
        ".fill 64, 1, 0\n\t" // Allocates a clean 64-byte user space text buffer initialized to 0
        
        ".align 4\n\t"
        ".user_init:\n\t"
        
        // --- 1. INVOKE SYS_OPEN ---
        "call .get_filename_addr\n\t"
        ".get_filename_addr:\n\t"
        "pop %%ebx\n\t"
        "sub $(.get_filename_addr - .filename_str), %%ebx\n\t" // EBX now holds pointer to "test.txt"
        
        "mov $3, %%eax\n\t" // SYS_OPEN = 3
        "int $0x80\n\t"     // Execute Syscall. Result (File Descriptor) is returned in EAX.
        
        // FIX: Save returned file descriptor into ESI instead of EBP
        "mov %%eax, %%esi\n\t" 
        
        // --- 2. INVOKE SYS_READ ---
        "mov %%esi, %%ebx\n\t" // Parameter 1 (EBX): File Descriptor ID
        
        "call .get_buffer_addr\n\t"
        ".get_buffer_addr:\n\t"
        "pop %%ecx\n\t"
        "sub $(.get_buffer_addr - .user_buffer), %%ecx\n\t" // Parameter 2 (ECX): Target Buffer Pointer
        
        "mov $32, %%edx\n\t"   // Parameter 3 (EDX): Read Size (32 bytes)
        "mov $4, %%eax\n\t"    // SYS_READ = 4
        "int $0x80\n\t"
        
        // --- 3. INVOKE SYS_PRINT_STR TO SHOW THE DATA ---
        "mov %%ecx, %%ebx\n\t" // Move our freshly populated buffer pointer into EBX
        "mov $2, %%eax\n\t"    // SYS_PRINT_STR = 2
        "int $0x80\n\t"
        
        // --- 4. EXIT PROCESS SAFELY ---
        "mov $0, %%ebx\n\t"
        "mov $0, %%eax\n\t"
        "int $0x80\n\t"
        
        :
        :
        : // FIX: Left completely empty! No inputs, outputs, or clobbers needed for raw naked assembly block
    );
}


void kmain(uint32_t magic, struct multiboot_info* bootInfo);

void kmain(uint32_t magic, struct multiboot_info* bootInfo){
    initGdt();
    print("GDT is done!\r\n");
    initIdt();
    initTimer();
    initKeyboard();

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

    // --- PASS THE HIGHLY ACCURATE PHYSICAL EXTRACTS ---
    initrd_init(tar_start, tar_end);

    spawn_user_task(&my_userspace_task);
    print("Userspace task registered.\n");

    asm_enable_interrupts();

    while(1) {
        for(volatile int i = 0; i < 5000000; i++);
    }
}
