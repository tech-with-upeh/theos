#ifndef TASK_H
#define TASK_H

#include "stdint.h"
#include "util.h" // Gives us access to your exact struct InterruptRegisters

typedef struct Task {
    uint32_t id;                             // Unique Process ID (PID)
    uint32_t page_directory;            // The CR3 value (paging) for this task
    struct InterruptRegisters *regs;     // Saved CPU registers state
    uint32_t is_alive; 
    
    // Pointers for memory cleanup tracking
    uint32_t* kernel_stack_ptr; 
    struct Task* next;                  // Pointer to the next task (Circular Loop)
} Task;

// Public system hooks
extern Task* current_task;
void init_multitasking();
Task* spawn_task(void (*func_ptr)());
Task* spawn_user_task(void (*func_ptr)());

#endif
