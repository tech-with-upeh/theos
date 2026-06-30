#ifndef ULIB_H
#define ULIB_H

#include "stdint.h"

// --- Low-Level System Call Wrappers ---
void exit(int code);
void putc(char c);
void puts(const char* str);

// --- Standard Formatting Library ---
void printf(const char* format, ...);

int open(const char* filename);
// Returns number of bytes read or -1 on error
int read(int fd, void* buffer, uint32_t size);

#endif
