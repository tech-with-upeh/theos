#include "syscall.h"
#include "task.h"
#include "vfs.h"
#include "stdlib/stdio.h"
#include "util.h"

void syscall_print_char(char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    printf(buf);
}

void syscall_print_string(const char* str) {
    printf((char*)str);
}

static inline void force_scheduler_yield() {
    __asm__ volatile("int $0x20");
}

void syscall_exit(int exit_code) {
    printf("\n[Process Exited with code: ");
    printf("%d, ",exit_code);
    printf("]\n");

    for (volatile int i = 0; i < 2000000; i++);
    current_task->is_alive = 0;
    force_scheduler_yield();
    while(1); 
}

// --- NEW VFS OPEN SYSTEM CALL ---
// Force external lookup access to the files mapped inside initrd.c
// (Make sure to include string utility headers if you have an strcmp equivalent, or we can use a basic look loop)
#define MAX_INITRD_FILES 32
extern struct vfs_node initrd_nodes[MAX_INITRD_FILES];
extern uint32_t initrd_file_count;

int syscall_open(const char* filename) {
    // Keep your excellent diagnostic prints active!
    printf("[DEBUG] User pointer address in EBX = %x\n", (uint32_t)filename);
    
    int file_index = -1;
    for (uint32_t i = 0; i < initrd_file_count; i++) {
        char* node_name = initrd_nodes[i].name;

        // --- ADD THIS DIAGNOSTIC PRINT ---
        printf("[DEBUG] Comparing user '%s' against RAMdisk file ID %d: ", filename, i);
        for(int d = 0; d < 15; d++) {
            char c = node_name[d];
            if (c >= 32 && c <= 126) printf("%c", c);
            else if (c == '\0') { printf("[\\0]"); break; }
            else printf("[%x]", (uint8_t)c);
        }
        printf("\n");
        // ---------------------------------
        
        if (node_name == 0) continue;

        int match = 1;
        int j = 0;
        
        // Loop until we find a difference, OR until BOTH strings hit their terminators
        while (1) {
            char f_char = filename[j];
            char n_char = node_name[j];

            // Normalize TAR padding spaces/slashes to a standard null terminator
            if (n_char == ' ' || n_char == '/') {
                n_char = '\0';
            }

            // If characters mismatch at this index, break immediately
            if (f_char != n_char) {
                match = 0;
                break;
            }

            // If BOTH strings successfully ended at the exact same index, it's a match!
            if (f_char == '\0' && n_char == '\0') {
                break;
            }

            j++;
        }
        
        if (match) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        printf("[Kernel Open Error]: File '%s' not found in ramdisk!\n", filename);
        return -1;
    }

    // Find a free slot in the current task's file descriptor table
    int fd_slot = -1;
    for (int i = 0; i < MAX_PROCESS_FILES; i++) {
        if (current_task->file_descriptors[i] == 0) {
            fd_slot = i;
            break;
        }
    }

    if (fd_slot == -1) {
        printf("[Kernel Open Error]: Task exceeded maximum file descriptors!\n");
        return -1;
    }

    // Bind the VFS node directly to this process's local handle slot
    current_task->file_descriptors[fd_slot] = &initrd_nodes[file_index];
    printf("[Kernel Open Success]: Assigned file '%s' to File Descriptor ID: %d\n", filename, fd_slot);

    return fd_slot;
}


// --- NEW VFS READ SYSTEM CALL ---
// Reads data from an open file descriptor index into a user-allocated buffer
int syscall_read(int fd, uint8_t* buffer, uint32_t size) {
    if (fd < 0 || fd >= MAX_PROCESS_FILES) return -1;
    
    struct vfs_node* node = current_task->file_descriptors[fd];
    if (node == 0) {
        printf("Syscall Error: Invalid file descriptor target %d\n", fd);
        return -1;
    }

    // Call our underlying abstract VFS framework router!
    // Offset is hardcoded to 0 for simplicity right now; we can track a custom read-head offset later.
    return vfs_read(node, 0, size, buffer);
}

// The core router that captures int 0x80
void handle_syscall(struct InterruptRegisters* regs) {
    uint32_t syscall_number = regs->eax;

    switch (syscall_number) {
        case SYS_EXIT:
            syscall_exit((int)regs->ebx);
            break;

        case SYS_PRINT_CHAR:
            syscall_print_char((char)regs->ebx);
            break;

        case SYS_PRINT_STR:
            syscall_print_string((const char*)regs->ebx);
            break;

        case SYS_OPEN:
            // Arguments passed to registers: EBX = filename string pointer
            // The return value of a system call is ALWAYS passed back via EAX!
            regs->eax = syscall_open((const char*)regs->ebx);
            break;

        case SYS_READ:
            // Arguments passed to registers: EBX = FD, ECX = Buffer Pointer, EDX = Size
            regs->eax = syscall_read((int)regs->ebx, (uint8_t*)regs->ecx, (uint32_t)regs->edx);
            break;

        default:
            printf("UNKNOWN SYSCALL TRIGGERED: ");
            printf("%d",syscall_number);
            printf("\n");
            break;
    }
}
