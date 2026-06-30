#include "stdint.h"
#include "multiboot.h"
#include "stdlib/stdio.h"
#include "util.h"
#include "memory.h"
#include "kmalloc.h"
#include "vga.h"

static uint32_t pageFrameMin;
static uint32_t pageFrameMax;
static uint32_t totalAlloc;
int mem_num_vpages;

#define NUM_PAGES_DIRS 256
#define NUM_PAGE_FRAMES (0x100000000 / 0x1000 / 8)

uint8_t physicalMemoryBitmap[NUM_PAGE_FRAMES / 8]; //Dynamically, bit array

static uint32_t pageDirs[NUM_PAGES_DIRS][1024] __attribute__((aligned(4096)));
static uint8_t pageDirUsed[NUM_PAGES_DIRS];

extern uint32_t initial_page_dir[];

uint32_t create_process_page_directory(void (*func_ptr)()) {
    // 1. Allocate and map the Process Page Directory Frame
    uint32_t dir_phys = pmmAllocPageFrame();
    printf("dir_phys=%x\n", dir_phys);

    uint32_t dir_scratch_vaddr = 0xE0000000;
    memMapPage(dir_scratch_vaddr, dir_phys, PAGE_FLAG_WRITE);
    uint32_t* new_dir_virt = (uint32_t*)dir_scratch_vaddr;
    memset(new_dir_virt, 0, 4096);

    // Clone baseline kernel higher-half mappings (0xC0000000 and above)
    for (int i = 768; i < 1024; i++) {
        new_dir_virt[i] = initial_page_dir[i];
    }

    // 2. Allocate a proper 4KB PAGE TABLE for PDE index 1 (virtual 0x00400000 - 0x007FFFFF)
    uint32_t user_pt_phys = pmmAllocPageFrame();
    // PDE entry points to our new page table: Present + Writable + User (0x07)
    new_dir_virt[1] = user_pt_phys | 0x07;

    // Map the new page table temporarily so the kernel can fill its entries
    uint32_t pt_scratch_vaddr = 0xE0002000;
    memMapPage(pt_scratch_vaddr, user_pt_phys, PAGE_FLAG_WRITE);
    uint32_t* user_pt = (uint32_t*)pt_scratch_vaddr;
    memset(user_pt, 0, 4096);

    // 3. Allocate physical memory for the user program code
    uint32_t user_code_phys = pmmAllocPageFrame();
    printf("user_code_phys=%x\n", user_code_phys);
    // Map it to PTE Index 0 -> Virtual Address: 0x00400000
    user_pt[0] = user_code_phys | 0x07; // Present + Writable + User

    // --- FIX 1: Allocate and map physical memory for the User Stack ---
    // Your raw_user_esp is 0x007FFFF0. This falls directly into the same PDE 1 table,
    // at the very last page table entry slot (Index 1023 covers 0x007FF000 to 0x007FFFFF).
    uint32_t user_stack_phys = pmmAllocPageFrame();
    printf("user_stack_phys=%x\n", user_stack_phys);
    user_pt[1023] = user_stack_phys | 0x07; // Present + Writable + User

    // Unmap the page table scratch address space now that BOTH entries are set
    uint32_t* pt_scratch_pt = REC_PAGETABLE(pt_scratch_vaddr >> 22);
    pt_scratch_pt[(pt_scratch_vaddr >> 12) & 0x3FF] = 0;
    invalidate(pt_scratch_vaddr);

    // 4. Copy user program code into the designated code frame via its own scratch mapping
    uint32_t code_scratch_vaddr = 0xE0001000;
    memMapPage(code_scratch_vaddr, user_code_phys, PAGE_FLAG_WRITE);
    memcpy((void*)code_scratch_vaddr, (void*)func_ptr, 4096);

    // Unmap the code scratch space
    uint32_t* code_pt = REC_PAGETABLE(code_scratch_vaddr >> 22);
    code_pt[(code_scratch_vaddr >> 12) & 0x3FF] = 0;
    invalidate(code_scratch_vaddr);

    // Unmap the directory scratch space
    uint32_t* dir_pt = REC_PAGETABLE(dir_scratch_vaddr >> 22);
    dir_pt[(dir_scratch_vaddr >> 12) & 0x3FF] = 0;
    invalidate(dir_scratch_vaddr);

    return dir_phys;
}


void pmm_init(uint32_t memLow, uint32_t memHigh){
    pageFrameMin = CEIL_DIV(memLow, 0x1000);
    pageFrameMax = memHigh / 0x1000;
    totalAlloc = 0;

    memset(physicalMemoryBitmap, 0, sizeof(physicalMemoryBitmap));
    
}

uint32_t* memGetCurrentPageDir(){
    uint32_t pd;
    asm volatile("mov %%cr3, %0": "=r"(pd));
    pd += KERNEL_START;

    return (uint32_t*) pd;
}

void memChangePageDir(uint32_t* pd){
    pd = (uint32_t*) (((uint32_t)pd)-KERNEL_START);
    asm volatile("mov %0, %%eax \n mov %%eax, %%cr3 \n" :: "m"(pd));
}

void syncPageDirs(){
    for (int i = 0; i < NUM_PAGES_DIRS; i++){
        if (pageDirUsed[i]){
            // Update the internal kernel tracking templates
            for (int j = 768; j < 1023; j++){
                pageDirs[i][j] = initial_page_dir[j] & ~PAGE_FLAG_OWNER;
            }
            
            // If this page directory is the one currently running inside CR3,
            // the hardware recursive structures automatically mirror the live initial_page_dir!
        }
    }
}


void memMapPage(uint32_t virutalAddr, uint32_t physAddr, uint32_t flags){
    uint32_t *prevPageDir = 0;

    if (virutalAddr >= KERNEL_START){
        prevPageDir = memGetCurrentPageDir();
        if (prevPageDir != initial_page_dir){
            memChangePageDir(initial_page_dir);
        }
    }

    uint32_t pdIndex = virutalAddr >> 22;
    uint32_t ptIndex = virutalAddr >> 12 & 0x3FF;
    
    uint32_t* pageDir = REC_PAGEDIR;
    uint32_t* pt = REC_PAGETABLE(pdIndex);

    if (!(pageDir[pdIndex] & PAGE_FLAG_PRESENT)){
        uint32_t ptPAddr = pmmAllocPageFrame();
        pageDir[pdIndex] = ptPAddr | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_OWNER | flags;
        invalidate(virutalAddr);

        for (uint32_t i = 0; i < 1024; i++){
            pt[i] = 0;
        }
    }

    pt[ptIndex] = physAddr | PAGE_FLAG_PRESENT | flags;
    mem_num_vpages++;
    invalidate(virutalAddr);

    if (prevPageDir != 0){
        syncPageDirs();

        if (prevPageDir != initial_page_dir){
            memChangePageDir(prevPageDir);
        }
    }

}

uint32_t pmmAllocPageFrame(){
    uint32_t start = pageFrameMin / 8 + ((pageFrameMin & 7) != 0 ? 1 : 0);
    uint32_t end = pageFrameMax / 8 - ((pageFrameMax & 7) != 0 ? 1 : 0);

    for (uint32_t b = start; b < end; b++){
        uint8_t byte = physicalMemoryBitmap[b];
        if (byte == 0xFF){
            continue;
        }

        for (uint32_t i = 0; i < 8; i++){
            bool_t used = byte >> i & 1;
            if (!used){
                byte ^= (-1 ^ byte) & (1 << i);
                physicalMemoryBitmap[b] = byte;
                totalAlloc++;

                //uint32_t addr = (b*8*i) * 0x1000;
                uint32_t addr = (b*8 + i) * 0x1000;
                return addr;
            }
        }
        
    }

    return 0;
}


void initMemory(uint32_t memHigh, uint32_t physicalAllocStart){
    mem_num_vpages = 0;
    // initial_page_dir[0] = 0;
    invalidate(0);
    initial_page_dir[1023] = ((uint32_t) initial_page_dir - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
    invalidate(0xFFFFF000);

    pmm_init(physicalAllocStart, memHigh);
    printf("pageFrameMin(phys)=%x", physicalAllocStart);
    memset(pageDirs, 0, 0x1000 * NUM_PAGES_DIRS);
    memset(pageDirUsed, 0, NUM_PAGES_DIRS);
}

void invalidate(uint32_t vaddr){
    asm volatile("invlpg %0" :: "m"(vaddr));
}


uint32_t memGetPhysicalAddr(uint32_t virtualAddr) {
    uint32_t pdIndex = virtualAddr >> 22;
    uint32_t ptIndex = (virtualAddr >> 12) & 0x3FF;

    uint32_t* pageDir = REC_PAGEDIR;
    if (!(pageDir[pdIndex] & PAGE_FLAG_PRESENT)) {
        return 0; // not mapped
    }

    uint32_t* pt = REC_PAGETABLE(pdIndex);
    if (!(pt[ptIndex] & PAGE_FLAG_PRESENT)) {
        return 0; // not mapped
    }

    uint32_t frame = pt[ptIndex] & 0xFFFFF000;
    uint32_t offset = virtualAddr & 0xFFF;
    return frame | offset;
}