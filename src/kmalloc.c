#include "stdint.h"
#include "util.h"
#include "memory.h"
#include "kmalloc.h"



static uint32_t heapStart;
static uint32_t heapSize;
static uint32_t threshold;
static bool_t kmallocInitalized = false;

void kmallocInit(uint32_t initialHeapSize){
    heapStart = KERNEL_MALLOC;
    heapSize = 0;
    threshold = 0;
    kmallocInitalized = true;

    changeHeapSize(initialHeapSize);
    *((uint32_t*)heapStart) = 0;
}

void changeHeapSize(int newSize){
    int oldPageTop = CEIL_DIV(heapSize, 0x1000);
    int newPageTop = CEIL_DIV(newSize, 0x1000);

    if (newPageTop > oldPageTop){
        int diff = newPageTop - oldPageTop;

        // Calculate exactly where the unmapped virtual space begins
        uint32_t currentMappingAddress = KERNEL_MALLOC + (oldPageTop * 0x1000);

        for (int i = 0; i < diff; i++){
            uint32_t phys = pmmAllocPageFrame();
            
            // Map exactly at the next sequential page address
            memMapPage(currentMappingAddress, phys, PAGE_FLAG_WRITE);
            
            // Advance to the next page boundary
            currentMappingAddress += 0x1000;
        }
    }

    heapSize = newSize;
}


// Track exactly how many bytes inside our mapped pool have been given out
static uint32_t heapAllocatedBytes = 0;

void* kmalloc(uint32_t size) {
    if (!kmallocInitalized) {
        return 0; // Guard against allocating before kmallocInit runs
    }

    // 1. Align the allocation size to 4-byte boundaries for the CPU
    if (size & 0x3) {
        size = (size & 0xFFFFFFFC) + 4;
    }

    // 2. Check if we need more virtual/physical pages to fit this new request
    if (heapAllocatedBytes + size > heapSize) {
        // Calculate the total absolute size we need the heap to become
        uint32_t requiredTotalSize = heapAllocatedBytes + size;
        
        // Expand the heap pages using your existing function!
        changeHeapSize(requiredTotalSize);
    }

    // 3. Calculate the exact virtual address for this specific chunk
    void* allocatedAddress = (void*)(heapStart + heapAllocatedBytes);

    // 4. Move our tracking offset forward so the next allocation doesn't overwrite this one
    heapAllocatedBytes += size;

    // Return the clean pointer back to your multitasking system
    return allocatedAddress;
}
