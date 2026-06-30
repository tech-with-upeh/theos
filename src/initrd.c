#include "initrd.h"
#include "kmalloc.h"
#include "memory.h"
#include "stdlib/stdio.h"
#include "util.h"

// Allocate a maximum number of files we can track on boot
#define MAX_INITRD_FILES 32
vfs_node_t initrd_nodes[MAX_INITRD_FILES];
vfs_node_t initrd_root;
uint32_t initrd_file_count = 0;

// Dynamic lookups of file data pointers in physical memory
uint32_t initrd_file_headers[MAX_INITRD_FILES];

// --- FIX: Create a permanent, globally visible storage pool for your file names ---
static char initrd_names[MAX_INITRD_FILES][64];

// --- THE VFS CALLBACK READ ROUTINE ---
static uint32_t initrd_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t file_index = node->inode;
    uint32_t raw_data_ptr = initrd_file_headers[file_index] + 512;

    if (offset >= node->size) return 0;
    if (offset + size > node->size) {
        size = node->size - offset;
    }

    memcpy(buffer, (void*)(raw_data_ptr + offset), size);
    return size;
}

vfs_node_t* initrd_init(uint32_t tar_phys_start, uint32_t tar_phys_end) {
    if (tar_phys_start == 0 || tar_phys_end == 0 || tar_phys_end <= tar_phys_start) {
        printf("initrd: Invalid physical TAR module bounds provided!\n");
        return 0;
    }

    uint32_t total_bytes = tar_phys_end - tar_phys_start;
    printf("initrd: Mapping TAR archive at physical %x - %x (%d bytes)\n", tar_phys_start, tar_phys_end, total_bytes);

    uint32_t tar_virt_base = 0xE1000000;
    for (uint32_t offset = 0; offset < total_bytes; offset += 0x1000) {
        memMapPage(tar_virt_base + offset, tar_phys_start + offset, PAGE_FLAG_WRITE);
    }

    initrd_root.type = VFS_DIRECTORY;
    initrd_root.size = 0;
    initrd_root.read = 0;
    initrd_root.write = 0;

    uint32_t address = tar_virt_base;
    while (address < (tar_virt_base + total_bytes)) {
        tar_header_t* header = (tar_header_t*)address;

        if (header->filename[0] == '\0') break;

        uint32_t file_size = 0;
        // Parse TAR size field
        for (int i = 0; i < 12; i++) {
            if (header->size[i] == '\0' || header->size[i] == ' ') break;
            file_size = (file_size << 3) + (header->size[i] - '0');
        }

        printf("initrd: Found file '%s', Size: %d bytes\n", header->filename, file_size);

        if (initrd_file_count < MAX_INITRD_FILES) {
            vfs_node_t* node = &initrd_nodes[initrd_file_count];
            
            // 1. Populate our globally persistent 2D string cache slot
            char* dest_name = initrd_names[initrd_file_count];
            int n_idx = 0;
            while (n_idx < 63 && header->filename[n_idx] != '\0' && header->filename[n_idx] != ' ') {
                dest_name[n_idx] = header->filename[n_idx];
                n_idx++;
            }
            dest_name[n_idx] = '\0'; // Enforce clean null termination

            // 2. COMPILER SAFE ASSIGNMENT OVERRIDE
            // If the size of node->name is 4, the compiler treats it as a standard 32-bit char* pointer.
            // If the size is larger, it treats it as an inline array. This prevents memory corruption!
            if (sizeof(node->name) == 4) {
                *(char**)(&(node->name)) = dest_name;
            } else {
                // It is an inline array! Copy the character string bytes directly into it.
                char* array_ptr = (char*)(&(node->name));
                int k = 0;
                while (dest_name[k] != '\0' && k < (int)sizeof(node->name) - 1) {
                    array_ptr[k] = dest_name[k];
                    k++;
                }
                array_ptr[k] = '\0';
            }
            
            node->type = VFS_FILE;
            node->size = file_size;
            node->inode = initrd_file_count;
            node->read = &initrd_read;
            node->write = 0;
            
            initrd_file_headers[initrd_file_count] = address;
            initrd_file_count++;
        }


        address += 512 + ((file_size + 511) & ~511);
    }

    vfs_root = &initrd_root;
    return vfs_root;
}
