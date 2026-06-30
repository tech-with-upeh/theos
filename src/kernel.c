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

static inline void asm_enable_interrupts() {
    __asm__ volatile("sti");
}

// "__attribute__((naked))" forces GCC to omit function prologues and epilogues.
void __attribute__((naked)) my_userspace_task() {
    __asm__ volatile (
        "jmp .user_init\n\t"
        
        ".filename_str:\n\t"
        ".ascii \"test.txt\\0\"\n\t"
        
        ".align 4\n\t"
        ".user_buffer1:\n\t"
        ".fill 16, 1, 0\n\t" // First 16-byte buffer zone
        
        ".align 4\n\t"
        ".user_buffer2:\n\t"
        ".fill 16, 1, 0\n\t" // Second 16-byte buffer zone
        
        ".align 4\n\t"
        ".user_init:\n\t"
        
        // --- 1. INVOKE SYS_OPEN ---
        "call .get_eip\n\t"
        ".get_eip:\n\t"
        "pop %%ebx\n\t" 
        "mov $.get_eip, %%ecx\n\t"
        "mov $.filename_str, %%edx\n\t"
        "sub %%edx, %%ecx\n\t" 
        "sub %%ecx, %%ebx\n\t" // EBX = Live pointer to "test.txt"
        
        "mov $3, %%eax\n\t" 
        "int $0x80\n\t"     
        "mov %%eax, %%esi\n\t" // ESI = Saved File Descriptor ID
        
        // --- 2. FIRST SYS_READ (Fetch 5 bytes into buffer 1) ---
        "mov %%esi, %%ebx\n\t" // FD
        "call .get_buf1\n\t"
        ".get_buf1:\n\t"
        "pop %%ecx\n\t"
        "mov $.get_buf1, %%eax\n\t"
        "mov $.user_buffer1, %%edx\n\t"
        "sub %%edx, %%eax\n\t"
        "sub %%eax, %%ecx\n\t" // ECX = Pointer to user_buffer1
        "mov $5, %%edx\n\t"    // Read size = 5 bytes
        "mov $4, %%eax\n\t"    // SYS_READ
        "int $0x80\n\t"
        
        // Print the first buffer chunk
        "mov %%ecx, %%ebx\n\t"
        "mov $2, %%eax\n\t"    // SYS_PRINT_STR
        "int $0x80\n\t"
        
        // --- 3. SECOND SYS_READ (Fetch the NEXT 5 bytes into buffer 2) ---
        "mov %%esi, %%ebx\n\t" // FD
        "call .get_buf2\n\t"
        ".get_buf2:\n\t"
        "pop %%ecx\n\t"
        "mov $.get_buf2, %%eax\n\t"
        "mov $.user_buffer2, %%edx\n\t"
        "sub %%edx, %%eax\n\t"
        "sub %%eax, %%ecx\n\t" // ECX = Pointer to user_buffer2
        "mov $5, %%edx\n\t"    // Read size = 5 bytes
        "mov $4, %%eax\n\t"    // SYS_READ
        "int $0x80\n\t"
        
        // Print the second buffer chunk
        "mov %%ecx, %%ebx\n\t"
        "mov $2, %%eax\n\t"    // SYS_PRINT_STR
        "int $0x80\n\t"
        
        // --- 4. EXIT PROCESS ---
        "mov $0, %%ebx\n\t"
        "mov $0, %%eax\n\t"
        "int $0x80\n\t"
        
        :
        :
        :
    );
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

    // --- PASS THE HIGHLY ACCURATE PHYSICAL EXTRACTS ---
    initrd_init(tar_start, tar_end);

    // spawn_user_task(&my_userspace_task);
    // print("Userspace task registered.\n");

    spawn_program("shell.bin");
    asm_enable_interrupts();
    initKeyboard();

    for(;;);
}
