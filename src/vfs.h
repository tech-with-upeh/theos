#ifndef VFS_H
#define VFS_H

#include "stdint.h"

#define VFS_FILE      0x01
#define VFS_DIRECTORY 0x02

struct vfs_node;

// These function pointer types define how the kernel talks to a storage driver
typedef uint32_t (*vfs_read_type)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef uint32_t (*vfs_write_type)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef void (*vfs_open_type)(struct vfs_node* node);
typedef void (*vfs_close_type)(struct vfs_node* node);

// The primary object representing any file, folder, or device block
typedef struct vfs_node {
    char* name;          // File name
    uint32_t type;           // VFS_FILE or VFS_DIRECTORY
    uint32_t size;           // File size in bytes
    uint32_t inode;          // A unique index used by the underlying filesystem layout
    
    // Function callbacks mapped to the actual filesystem driver code
    vfs_read_type read;
    vfs_write_type write;
    vfs_open_type open;
    vfs_close_type close;
} vfs_node_t;

// The global root directory placeholder of our entire operating system
extern vfs_node_t* vfs_root;

// Kernel filesystem function interfaces
uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);

#endif
