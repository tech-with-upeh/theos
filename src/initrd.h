#ifndef INITRD_H
#define INITRD_H

#include "stdint.h"
#include "vfs.h"

// Standard POSIX TAR Header Layout
typedef struct tar_header {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];     // File size stored as an ASCII octal string!
    char mtime[12];
    char chksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} __attribute__((packed)) tar_header_t;

// Multiboot structure for modules passed by GRUB
typedef struct multiboot_module {
    uint32_t mod_start;   // Physical address where the tar file begins
    uint32_t mod_end;     // Physical address where it ends
    uint32_t cmdline;
    uint32_t pad;
} __attribute__((packed)) multiboot_module_t;

// Function interfaces
vfs_node_t* initrd_init(uint32_t phys_mods_addr, uint32_t mods_count);

#endif
