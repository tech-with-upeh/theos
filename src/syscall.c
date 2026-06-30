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
    printf("[Kernel Open]: User space requested file: %s\n", filename);
    
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
        printf("[Kernel Open Error]: File '%s' not found in ramdisk!\n", filename);
        return -1;
    }

    // Find a free handle slot in the current task descriptor array
    int fd_slot = -1;
    for (int i = 0; i < MAX_PROCESS_FILES; i++) {
        // Checking if the node pointer inside the handle structure is empty (0)
        if (current_task->file_descriptors[i].node == 0) {
            fd_slot = i;
            break;
        }
    }

    if (fd_slot == -1) {
        printf("[Kernel Open Error]: Task exceeded maximum file descriptors!\n");
        return -1;
    }

    // --- FIX: Assign node and initialize the streaming offset ---
    current_task->file_descriptors[fd_slot].node = &initrd_nodes[file_index];
    current_task->file_descriptors[fd_slot].offset = 0; // Fresh open starts at the beginning
    
    printf("[Kernel Open Success]: Assigned file '%s' to File Descriptor ID: %d\n", filename, fd_slot);
    return fd_slot;
}

int syscall_read(int fd, uint8_t* buffer, uint32_t size) {
    if (fd < 0 || fd >= MAX_PROCESS_FILES) return -1;
    
    // Grab a pointer to the process file handle object
    file_handle_t* handle = &current_task->file_descriptors[fd];
    if (handle->node == 0) {
        printf("Syscall Error: Invalid file descriptor target %d\n", fd);
        return -1;
    }

    // --- STREAMING OFFSET MATH ---
    // Pass the saved runtime offset into your existing VFS framework router
    uint32_t bytes_read = vfs_read(handle->node, handle->offset, size, buffer);

    // Push the file offset pointer forward by the number of bytes successfully retrieved
    handle->offset += bytes_read;

    // Return the actual number of bytes read back to user space (EAX)
    return (int)bytes_read;
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
