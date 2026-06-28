#include "task.h"
#include "memory.h" // Gives us kmalloc and memGetCurrentPageDir
#include "util.h"
#include "kmalloc.h"
#include "vga.h"
#include "stdlib/stdio.h"

Task* ready_queue = 0;
Task* current_task = 0;
uint32_t next_pid = 1;

// Initialize multitasking by turning the core kernel into Task 0
// void init_multitasking() {
//     Task* main_task = (Task*)kmalloc(sizeof(Task));
//     main_task->id = 0;
//     main_task->page_directory = (uint32_t)memGetCurrentPageDir();
    
//     main_task->next = main_task; // Circle points back to itself

//     current_task = main_task;
//     ready_queue = main_task;
// }
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
    main_task->page_directory = (uint32_t)memGetCurrentPageDir();
    main_task->next = main_task; // Circle points back to itself

    // 2. Clear out our dedicated static register tracking block
    memset(&kernel_initial_regs, 0, sizeof(struct InterruptRegisters));

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
    uint32_t stack_size = 4096;
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



// Create a new task that executes a specific C function
// Task* spawn_task(void (*func_ptr)()) {
//         print("spawning task\n");
   
//     // 1. Allocate space for the task control block card using your working kmalloc
//     Task* new_task = (Task*)kmalloc(sizeof(Task));
//     if (new_task == 0) {
//     print("CRITICAL ERROR: kmalloc returned NULL pointer inside spawn_task!\n");
//     while(1); // Freeze here so you can read the error message on screen
// }
    
//     print("spawning task:malloc\n");
//     // 2. Allocate a dedicated 4KB stack for this specific task's math and local variables
//     uint32_t* stack = (uint32_t*)kmalloc(4096);
    
//     // 3. Stacks grow downward in x86 memory! Point to the very top edge of the 4KB block
//     uint32_t* esp = (uint32_t*)((uint32_t)stack + 4096);
    
//     // 4. Manually construct the exact stack frame that an 'iret' or pop sequence expects.
//     // This MUST match the layout of your 'struct InterruptRegisters' from bottom to top!
    
//     *(--esp) = 0x10;                // ss
//     *(--esp) = (uint32_t)esp;       // useresp
//     *(--esp) = 0x0202;              // eflags
//     *(--esp) = 0x08;                // csm
//     *(--esp) = (uint32_t)func_ptr;  // eip
    
//     *(--esp) = 0;                   // err_code
//     *(--esp) = 0;                   // int_no
    
//     // General Purpose Registers unrolled in exact reverse order of your struct:
//     *(--esp) = 0;                   // eax
//     *(--esp) = 0;                   // ecx
//     *(--esp) = 0;                   // edx
//     *(--esp) = 0;   
//      print("spawning task:esp1\n");                // ebx
//     *(--esp) = (uint32_t)esp;       // esp
//     *(--esp) = 0;                   // ebp
//     *(--esp) = 0;                   // esi
//     *(--esp) = 0;                   // edi
    
//     *(--esp) = 0x10;                // ds
//     *(--esp) = 0;                   // cr2

//      print("spawning task:lastttttt\n");

//     // 5. Fill out the rest of the Task information card
//     // print_uint(current_task->id);
//     if (current_task == 0)
//     {
//         print("Current task is 0\n");
//     }
//     else
//     {
//         print_uint((uint32_t)current_task);
//         print("\nCurrent Task is not 0\n");
//         print_uint(current_task->id);
//     }
    
//     new_task->id = current_task->id++;
//     print("spawning task:pid++\n");
//     new_task->page_directory = (uint32_t)memGetCurrentPageDir();
    
//      print("spawning task:pgd");
//     // Safely point our saved registers data directly to this freshly forged stack layout
//     new_task->regs = *(struct InterruptRegisters*)esp;
//      print("spawning task:regs");
//     // 6. Thread this new task directly into our circular linked carousel chain
//     new_task->next = ready_queue->next;
//     ready_queue->next = new_task;
//      print("spawning task:end\n");
//     return new_task;
// }


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
