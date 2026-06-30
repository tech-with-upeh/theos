#include "ulib.h"
#include <stdarg.h> // Standard cross-compiler header for variable arguments (...)

void exit(int code) {
    __asm__ volatile("mov %0, %%ebx\n\t"
                     "mov $0, %%eax\n\t" // SYS_EXIT = 0
                     "int $0x80" : : "r"(code) : "eax", "ebx");
}

void putc(char c) {
    __asm__ volatile("mov %0, %%ebx\n\t"
                     "mov $1, %%eax\n\t" // SYS_PRINT_CHAR = 1
                     "int $0x80" : : "r"((uint32_t)c) : "eax", "ebx");
}

void puts(const char* str) {
    __asm__ volatile("mov %0, %%ebx\n\t"
                     "mov $2, %%eax\n\t" // SYS_PRINT_STR = 2
                     "int $0x80" : : "r"(str) : "eax", "ebx");
}

// Low-overhead integer to string text rendering assistant
static void print_int(int value, int radix, int is_signed) {
    char buffer[32];
    int i = 0;
    unsigned int u_val = value;

    if (is_signed && value < 0) {
        putc('-');
        u_val = -value;
    }

    // Convert value characters backwards into the local buffer stack
    do {
        unsigned int remainder = u_val % radix;
        if (remainder < 10) {
            buffer[i++] = '0' + remainder;
        } else {
            buffer[i++] = 'a' + (remainder - 10);
        }
        u_val /= radix;
    } while (u_val > 0);

    // Unwind the buffer forward to output characters in the correct order
    while (i > 0) {
        putc(buffer[--i]);
    }
}

// Comprehensive custom printf routing engine
void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            putc(format[i]);
            continue;
        }

        // Advance past the '%' character marker
        i++; 
        
        switch (format[i]) {
            case '%':
                putc('%');
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                putc(c);
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                if (s == 0) s = "(null)";
                puts(s);
                break;
            }
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                print_int(val, 10, 1); // Base 10, Signed numbers
                break;
            }
            case 'x': {
                int val = va_arg(args, int);
                print_int(val, 16, 0); // Base 16 Hexadecimal
                break;
            }
            case 'p': {
                uint32_t val = va_arg(args, uint32_t);
                puts("0x");
                print_int((int)val, 16, 0); // Pointer notation address
                break;
            }
            default:
                // Unsupported symbol fallback catch-all
                putc('%');
                putc(format[i]);
                break;
        }
    }
    va_end(args);
}


int open(const char* filename) {
    int result;
    __asm__ volatile(
        "mov $3, %%eax\n\t"    // SYS_OPEN = 3
        "mov %1, %%ebx\n\t"    // Parameter 1: Filename string address
        "int $0x80\n\t"
        "mov %%eax, %0"        // Grab returned File Descriptor from EAX
        : "=r"(result)
        : "r"(filename)
        : "eax", "ebx"
    );
    return result;
}

int read(int fd, void* buffer, uint32_t size) {
    int result;

    // Force GCC to place the variables into the exact hardware registers required by the kernel
    register uint32_t syscall_num __asm__("eax") = 4; // SYS_READ = 4
    register int      arg_fd      __asm__("ebx") = fd;
    register void*    arg_buf     __asm__("ecx") = buffer;
    register uint32_t arg_size    __asm__("edx") = size;

    __asm__ volatile(
        "int $0x80"
        : "=a"(result) /* The output comes back in EAX, so we bind it to result */
        : "r"(syscall_num), "r"(arg_fd), "r"(arg_buf), "r"(arg_size)
        : "memory"     /* Tell GCC that memory was modified by this read block */
    );

    return result;
}

