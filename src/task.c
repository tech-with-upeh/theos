#include "task.h"
#include "memory.h" // Gives us kmalloc and memGetCurrentPageDir
#include "util.h"
#include "kmalloc.h"
#include "vga.h"
#include "vfs.h"
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

    memset(main_task->file_descriptors, 0, sizeof(main_task->file_descriptors));

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


// Forward declare your VFS initialization hooks or include relevant headers
#define MAX_INITRD_FILES 32
extern struct vfs_node initrd_nodes[MAX_INITRD_FILES];
extern uint32_t initrd_file_count;
extern uint32_t initrd_file_headers[MAX_INITRD_FILES];

Task* spawn_program(const char* filename) {
    // 1. Locate the program file in your parsed RAMdisk table
    int file_index = -1;
    for (uint32_t i = 0; i < initrd_file_count; i++) {
        char* node_name = initrd_nodes[i].name;
        if (node_name == 0) continue;

        int match = 1;
        int j = 0;
        while (1) {
            char f_char = filename[j];
            char n_char = node_name[j];
            if (n_char == ' ' || n_char == '/') n_char = '\0';
            if (f_char != n_char) { match = 0; break; }
            if (f_char == '\0' && n_char == '\0') break;
            j++;
        }
        if (match) { file_index = i; break; }
    }

    if (file_index == -1) {
        printf("Program Loader Error: Binary application '%s' not found on disk!\n", filename);
        return 0;
    }

    // 2. Allocate our standard operating system Task descriptor structure card
    Task* new_task = (Task*)kmalloc(sizeof(Task));
    memset(new_task->file_descriptors, 0, sizeof(new_task->file_descriptors));

    uint32_t* kernel_stack = (uint32_t*)kmalloc(8192);
    uint32_t* esp = (uint32_t*)((uint32_t)kernel_stack + 8192);

    // 3. Allocate a distinct virtual memory workspace directory for this executable
    uint32_t process_physical_cr3 = pmmAllocPageFrame();
    
    // Map it temporarily so we can copy kernel templates into it
    uint32_t dir_scratch_vaddr = 0xE0000000;
    memMapPage(dir_scratch_vaddr, process_physical_cr3, PAGE_FLAG_WRITE);
    uint32_t* new_dir_virt = (uint32_t*)dir_scratch_vaddr;
    memset(new_dir_virt, 0, 4096);

    // Sync kernel base mappings across to ensure higher-half stability
    for (int i = 768; i < 1024; i++) {
        new_dir_virt[i] = initial_page_dir[i];
    }

    // 4. Set up page tables for User Code space (0x00400000)
    uint32_t user_pt_phys = pmmAllocPageFrame();
    new_dir_virt[1] = user_pt_phys | 0x07; // Present + Writable + User

    uint32_t pt_scratch_vaddr = 0xE0002000;
    memMapPage(pt_scratch_vaddr, user_pt_phys, PAGE_FLAG_WRITE);
    uint32_t* user_pt = (uint32_t*)pt_scratch_vaddr;
    memset(user_pt, 0, 4096);

    // 5. Allocate the real memory page frames where the program binary code bytes will live
    uint32_t user_code_phys = pmmAllocPageFrame();
    user_pt[0] = user_code_phys | 0x07; // Map 0x00400000

    // Allocate physical space backing for User Stack pointer execution
    uint32_t user_stack_phys = pmmAllocPageFrame();
    user_pt[1023] = user_stack_phys | 0x07; // Map 0x007FF000

    // Clear temporary scratch mapping tracks
    uint32_t* pt_scratch_pt = REC_PAGETABLE(pt_scratch_vaddr >> 22);
    pt_scratch_pt[(pt_scratch_vaddr >> 12) & 0x3FF] = 0;
    invalidate(pt_scratch_vaddr);

    // 6. READ binary bytes straight from the ramdisk archive memory pool into the fresh code page
    uint32_t code_scratch_vaddr = 0xE0001000;
    memMapPage(code_scratch_vaddr, user_code_phys, PAGE_FLAG_WRITE);
    
    // Calculate exactly where the file's raw payload bytes are sitting inside your TAR pages
    // externally looked up via your existing initrd_file_headers tracker array
    uint32_t file_raw_data_ptr = initrd_file_headers[file_index] + 512;
    uint32_t file_size = initrd_nodes[file_index].size;

    // Direct memory block copy into the process's destination code space page buffer!
    memset((void*)code_scratch_vaddr, 0, 4096);
    memcpy((void*)code_scratch_vaddr, (void*)file_raw_data_ptr, file_size);

    // Close remaining scratch tracks
    uint32_t* code_pt = REC_PAGETABLE(code_scratch_vaddr >> 22);
    code_pt[(code_scratch_vaddr >> 12) & 0x3FF] = 0;
    invalidate(code_scratch_vaddr);

    uint32_t* dir_pt = REC_PAGETABLE(dir_scratch_vaddr >> 22);
    dir_pt[(dir_scratch_vaddr >> 12) & 0x3FF] = 0;
    invalidate(dir_scratch_vaddr);

    // 7. Forge the hardware CPL=3 context stack framework frame backwards
    uint32_t raw_user_esp = 0x007FFFF0;
    *(--esp) = 0x23;                // ss
    *(--esp) = raw_user_esp;        // useresp
    *(--esp) = 0x00000202;          // eflags
    *(--esp) = 0x1B;                // cs
    *(--esp) = 0x00400000;          // eip (Always points directly to the start of the execution page!)

    *(--esp) = 0;                   // err_code
    *(--esp) = 32;                  // int_no

    *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; // eax, ecx, edx, ebx
    *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; *(--esp) = 0; // esp, ebp, esi, edi
    *(--esp) = 0x23;                // ds
    *(--esp) = 0;                   // cr2

    // Populate Task metadata
    new_task->id = next_pid++;
    new_task->page_directory = process_physical_cr3;
    new_task->kernel_stack_ptr = kernel_stack;
    new_task->regs = (struct InterruptRegisters*)esp;
    new_task->is_alive = 1;

    // Thread task into carousel run-queue chain
    new_task->next = current_task->next;
    current_task->next = new_task;

    printf("[Program Loader]: Successfully spawned independent executable '%s' with PID %d!\n", filename, new_task->id);
    return new_task;
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
    
    memset(new_task->file_descriptors, 0, sizeof(new_task->file_descriptors));

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

    memset(new_task->file_descriptors, 0, sizeof(new_task->file_descriptors));

    new_task->next = current_task->next;
    current_task->next = new_task;

    return new_task;
}

