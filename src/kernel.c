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

#include "libc/ulib.h"

static inline void asm_enable_interrupts() {
    __asm__ volatile("sti");
}

// "__attribute__((naked))" forces GCC to omit function prologues and epilogues.
// This completely stops stack frame register pollution during raw code copying!
void __attribute__((naked)) my_userspace_task() {
    __asm__ volatile (
        "jmp .user_code_start\n\t"
        
        // Embed the string bytes cleanly inside the function memory pool boundary!
        ".my_user_string:\n\t"
        ".ascii \"Hello from clean INLINED ulib wrapper!\\n\\0\"\n\t"
        
        ".align 4\n\t"
        ".user_code_start:\n\t"
        
        // 1. Calculate the string address relative to EIP using standard x86 math
        "call .get_eip_offset\n\t"
        ".get_eip_offset:\n\t"
        "pop %%ebx\n\t"
        "sub $(.get_eip_offset - .my_user_string), %%ebx\n\t" // EBX now holds the raw user-space pointer to the string!
        
        // 2. Invoke Syscall 2 (SYS_PRINT_STR) directly without using stack wrappers
        "mov $2, %%eax\n\t"
        "int $0x80\n\t"
        
        // 3. Invoke Syscall 0 (SYS_EXIT) to park the process safely
        "mov $0, %%ebx\n\t"
        "mov $0, %%eax\n\t"
        "int $0x80\n\t"
        
        :
        :
        : "eax", "ebx"
    );
}


void kmain(uint32_t magic, struct multiboot_info* bootInfo);

void kmain(uint32_t magic, struct multiboot_info* bootInfo){
    initGdt();
    print("GDT is done!\r\n");
    initIdt();
    initTimer();
    initKeyboard();

    // Read ALL bootInfo fields RIGHT NOW, before initMemory kills the identity map
    uint32_t mem_upper      = bootInfo->mem_upper;
    uint32_t flags          = bootInfo->flags;
    uint32_t mods_count     = bootInfo->mods_count;
    uint32_t mods_addr      = bootInfo->mods_addr;

    printf("flags=%x mods_count=%x mods_addr=%x\n", flags, mods_count, mods_addr);

    uint32_t physicalAllocStart = 0x00200000;

    // NOW it's safe — all bootInfo reads are done
    initMemory(mem_upper * 1024, physicalAllocStart);

    // These now use local variables, not bootInfo pointer — safe after identity map removal
    printf("physicalAllocStart=%x\n", physicalAllocStart);
    printf("mem_upper_bytes=%x\n", mem_upper * 1024);

    kmallocInit(0x1000);
    init_multitasking();
    print("Memory allocation done!\n");

    spawn_user_task(&my_userspace_task);
    print("Userspace task registered.\n");

    asm_enable_interrupts();

    while(1) {
        for(volatile int i = 0; i < 5000000; i++);
    }
}