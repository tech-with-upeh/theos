#include "stdint.h"
#include "util.h"

void memset(void *dest, char val, uint32_t count){
    char *temp = (char*) dest;
    for (; count != 0; count --){
        *temp++ = val;
    }

}

void* memcpy(void* dest, const void* src, uint32_t count) {
    char* dst_ptr = (char*)dest;
    const char* src_ptr = (const char*)src;

    for (uint32_t i = 0; i < count; i++) {
        dst_ptr[i] = src_ptr[i];
    }

    return dest;
}


void outPortB(uint16_t Port, uint8_t Value){
    asm volatile ("outb %1, %0" : : "dN" (Port), "a" (Value));
}

char inPortB(uint16_t port){
    char rv;
    asm volatile("inb %1, %0": "=a"(rv):"dN"(port));
    return rv;
}

void outPortL(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t inPortL(uint16_t port) {
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}
