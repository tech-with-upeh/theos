#ifndef ULIB_H
#define ULIB_H

#include "stdint.h"

#define SYS_EXIT         0
#define SYS_PRINT_CHAR   1
#define SYS_PRINT_STR    2

// By making this static inline, its bytes are compiled DIRECTLY inside the user task
static inline void u_print_char(char c) {
    __asm__ volatile (
        "mov %0, %%ebx\n\t"     // Move character value into EBX
        "mov $1, %%eax\n\t"     // Syscall 1: SYS_PRINT_CHAR
        "int $0x80\n\t"         // Invoke kernel trap
        :
        : "r"((uint32_t)c)
        : "eax", "ebx"
    );
}

static inline void u_print_string(const char* str) {
    __asm__ volatile (
        "mov %0, %%ebx\n\t"     // Move string pointer address into EBX
        "mov $2, %%eax\n\t"     // Syscall 2: SYS_PRINT_STR
        "int $0x80\n\t"         // Invoke kernel trap
        :
        : "r"(str)
        : "eax", "ebx"
    );
}

static inline void u_exit(int code) {
    __asm__ volatile (
        "mov %0, %%ebx\n\t"     // Move exit code integer into EBX
        "mov $0, %%eax\n\t"     // Syscall 0: SYS_EXIT
        "int $0x80\n\t"         // Invoke kernel trap
        :
        : "r"((uint32_t)code)
        : "eax", "ebx"
    );
}

#endif
