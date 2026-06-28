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

static inline void asm_enable_interrupts() {
    __asm__ volatile("sti");
}


// Define a test function for our second thread to execute
void my_parallel_task() {
    while(1) {
        // Print character to screen using your vga.h helper
        print("hello task1"); 
        
        // Loop heavily to slow down the text output visually
        for(volatile int i = 0; i < 5000000; i++); 
    }
}

void kmain(uint32_t magic, struct multiboot_info* bootInfo);

void kmain(uint32_t magic, struct multiboot_info* bootInfo){
    initGdt();
    print("GDT is done!\r\n");
    initIdt();
    initTimer();
    initKeyboard();

    uint32_t mod1 = *(uint32_t*)(bootInfo->mods_addr + 4);
    uint32_t physicalAllocStart = (mod1 + 0xFFF) & ~0xFFF;

    initMemory(bootInfo->mem_upper * 1024, physicalAllocStart);
    kmallocInit(0x1000);
    init_multitasking();
    print("Memory allocation done!");

    // 4. Spawn our parallel worker thread (Task 1)
    spawn_task(&my_parallel_task);

    print("Spawned-task\n");
    
    asm_enable_interrupts();
    printf("Spawned task\n");
    // 5. Initialize your IDT, Timer (IRQ0), and Keyboard (IRQ1) here...
    // initIdt();
    // initTimer(); // This turns on the scheduler interrupts!

    // 6. Task 0 (Main Kernel Loop) continues executing here
    while(1) {
        print("main task");
        for(volatile int i = 0; i < 5000000; i++);
    }

    for(;;);
}
