#ifndef SYSCALL_H
#define SYSCALL_H

#include "interrupts/idt.h" // Gives us access to struct InterruptRegisters

#define SYS_EXIT         0
#define SYS_PRINT_CHAR   1
#define SYS_PRINT_STR    2
#define SYS_OPEN         3
#define SYS_READ         4

struct InterruptRegisters;

// Define the central system call router function signature
void handle_syscall(struct InterruptRegisters* regs);

#endif
