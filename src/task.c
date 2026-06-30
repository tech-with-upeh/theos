#include "task.h"
#include "memory.h" // Gives us kmalloc and memGetCurrentPageDir
#include "util.h"
#include "kmalloc.h"
#include "vga.h"
#include "stdlib/stdio.h"

Task* ready_queue = 0;
Task* current_task = 0;
uint32_t next_pid = 1;


// Keep a safe, static global structure instance for Task 0's initial save target
static struct InterruptRegisters kernel_initial_regs;

void init_multitasking() {
    // 1. Allocate the primary task card
    Task* main_task = (Task*)kmalloc(sizeof(Task));
    if (main_task == 0) {
        print("CRITICAL: Failed to allocate main task structures!\n");
        while(1);
    }
    
    main_task->id = 0;
    //main_task->page_directory = (uint32_t)memGetCurrentPageDir();
    // Force Task 0 to track the physical address of the kernel boot directory
    main_task->page_directory = (uint32_t)&initial_page_dir - 0xC0000000;
    main_task->next = main_task; // Circle points back to itself

    // 2. Clear out our dedicated static register tracking block
    memset(&kernel_initial_regs, 0, sizeof(struct InterruptRegisters));

    main_task->is_alive = 1; 
    main_task->kernel_stack_ptr = 0;
    // 3. Point the task card's regs pointer to this safe memory area
    main_task->regs = &kernel_initial_regs;

    // 4. Populate default Ring 0 segment values inside that target memory
    main_task->regs->csm = 0x08;      // Kernel Code Segment Selector
    main_task->regs->ds = 0x10;       // Kernel Data Segment Selector
    main_task->regs->eflags = 0x202;  // Interrupts Enabled configuration

    // Establish global pointer lookups
    current_task = main_task;
    ready_queue = main_task;
}




Task* spawn_task(void (*func_ptr)()) {
    Task* new_task = (Task*)kmalloc(sizeof(Task));
    if (new_task == 0) {
        print("CRITICAL ERROR: Task allocation failed!\n");
        while(1);
    }
    
    // 1. Allocate a standard, safe 8KB stack block
    uint32_t stack_size = 8192;
    uint32_t* stack = (uint32_t*)kmalloc(stack_size);
    
    // 2. CRITICAL MATH FIX: 
    // Calculate the top of the stack using raw integers FIRST.
    // This ensures your pointer starts at 0x0D002000 instead of 0x0D000078.
    uint32_t raw_top_of_stack = (uint32_t)stack + stack_size;
    
    // 3. Align the stack pointer to an 8-byte boundary for CPU stability
    raw_top_of_stack &= 0xFFFFFFF8;
    
    // 4. Cast it back to our working stack pointer
    uint32_t* esp = (uint32_t*)raw_top_of_stack;
    printf("new_task=%x stack=%x esp_top=%x\n", (uint32_t)new_task, (uint32_t)stack, (uint32_t)((uint32_t)stack+16384));
    
    // --- CORRECT RING 0 HARDWARE FRAME ---
    *(--esp) = 0x00000202;          // eflags (Interrupts Enabled, Bit 1 set)
    *(--esp) = 0x08;                // cs (Kernel Code Segment Selector)
    *(--esp) = (uint32_t)func_ptr;  // eip (Where the task begins running)
    
    // Pushed by the macro wrappers
    *(--esp) = 0;                   // err_code placeholder
    *(--esp) = 32;                  // int_no placeholder (Timer interrupt)
    
    // General purpose registers pushed by pusha/popa sequence
    *(--esp) = 0;                   // eax
    *(--esp) = 0;                   // ecx
    *(--esp) = 0;                   // edx
    *(--esp) = 0;                   // ebx
    *(--esp) = 0;                   // esp value placeholder (ignored by popa)
    *(--esp) = 0;                   // ebp
    *(--esp) = 0;                   // esi
    *(--esp) = 0;                   // edi
    
    // Segment tracking variables pushed manually by irq_common_stub
    *(--esp) = 0x10;                // ds segment selector
    *(--esp) = 0;                   // cr2 placeholder
    // Fill out metadata
    new_task->id = current_task->id + 1;
    new_task->page_directory = (uint32_t)memGetCurrentPageDir();
    
    // Point saved registers block to our custom forged frame
    new_task->regs = (struct InterruptRegisters*)esp;
    
    // Thread task inside our circular carousel chain
    new_task->next = ready_queue->next;
    ready_queue->next = new_task;
    
    return new_task;
}

// Task* spawn_user_task(void (*func_ptr)()) {
//     // 1. Allocate the task card structure card
//     Task* new_task = (Task*)kmalloc(sizeof(Task));
//     if (new_task == 0) {
//         print("CRITICAL ERROR: User Task allocation failed!\n");
//         while(1);
//     }
    
//     // 2. Allocate the Ring 0 Interrupt Stack boundary
//     uint32_t* kernel_stack = (uint32_t*)kmalloc(8192);
//     uint32_t* esp = (uint32_t*)((uint32_t)kernel_stack + 8192);
    
//     uint32_t raw_user_esp = 0x007FFFF0; 

//     // 3. Build the hardware execution context backwards
//     *(--esp) = 0x23;                // ss
//     *(--esp) = raw_user_esp;        // useresp
//     *(--esp) = 0x00000202;          // eflags
//     *(--esp) = 0x1B;                // cs
//     *(--esp) = (uint32_t)func_ptr;  // eip (0x00400000)
    
//     *(--esp) = 0;                   // err_code
//     *(--esp) = 32;                  // int_no
    
//     *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; // eax, ecx, edx, ebx
//     *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; // esp, ebp, esi, edi
    
//     *(--esp) = 0x23;                // ds
//     *(--esp) = 0;                   // cr2
    
//     // 4. Populate all metadata fields explicitly
//     new_task->id = next_pid++;
//     new_task->page_directory = (uint32_t)memGetCurrentPageDir();
//     new_task->kernel_stack_ptr = kernel_stack;
//     new_task->regs = (struct InterruptRegisters*)esp;
    
//     // CRITICAL: Guarantee this field is written cleanly before linking
//     new_task->is_alive = 1; 

//     // 5. Thread the task card directly into the current execution loop lane!
//     // By using current_task instead of ready_queue, we ensure immediate rotation stability.
//     new_task->next = current_task->next;
//     current_task->next = new_task;
    
//     return new_task;
// }

// Declare an external hook to let us get the raw physics/virtual address of current dir
extern uint32_t* memGetCurrentPageDir();

Task* spawn_user_task(void (*func_ptr)()) {
    Task* new_task = (Task*)kmalloc(sizeof(Task));
    if (new_task == 0) {
        print("CRITICAL ERROR: User Task allocation failed!\n");
        while(1);
    }

    uint32_t* kernel_stack = (uint32_t*)kmalloc(8192);
    uint32_t* esp = (uint32_t*)((uint32_t)kernel_stack + 8192);

    // Directory creation + code copy now happens entirely inside this one call
    uint32_t process_physical_cr3 = create_process_page_directory(func_ptr);

    uint32_t raw_user_esp = 0x007FFFF0;
    *(--esp) = 0x23;                // ss
    *(--esp) = raw_user_esp;        // useresp
    *(--esp) = 0x00000202;          // eflags
    *(--esp) = 0x1B;                // cs
    *(--esp) = 0x00400000;          // eip — always the start of the mapped user page

    *(--esp) = 0;                   // err_code
    *(--esp) = 32;                  // int_no

    *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; // eax, ecx, edx, ebx
    *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; // esp, ebp, esi, edi
    *(--esp) = 0x23;                // ds
    *(--esp) = 0;                   // cr2

    new_task->id = next_pid++;
    new_task->page_directory = process_physical_cr3;
    new_task->kernel_stack_ptr = kernel_stack;
    new_task->regs = (struct InterruptRegisters*)esp;
    new_task->is_alive = 1;

    new_task->next = current_task->next;
    current_task->next = new_task;

    return new_task;
}


// Called by your assembly interrupt handler on every timer tick
// uint32_t schedule(uint32_t current_esp) {
//     if (current_task == 0) return current_esp;

//     // 1. Save the current CPU state passed from the assembly handler into the running task
//     current_task->regs = (struct InterruptRegisters*)current_esp;

//     // 2. Rotate the circular linked list carousel to the next ready task
//     current_task = current_task->next;

//     // 3. Hand back the new task's stack pointer so assembly can load its registers
//     // We must return a pointer to where the InterruptRegisters struct begins on its stack
//     return (uint32_t)&(current_task->regs);
// }
