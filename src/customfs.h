#ifndef CUSTOM_FS_H
#define CUSTOM_FS_H

#include "stdint.h"

#define CUSTOM_FS_MAGIC 0x4D594F53 
#define MAX_FILENAME    47  // Allow 46 characters + 1 null terminator

typedef struct {
    uint32_t magic;         
    uint32_t total_sectors; 
    uint32_t file_count;    
    uint8_t  reserved[500]; // Explicitly pads superblock structure to exactly 512 bytes
} __attribute__((packed)) superblock_t;

// Exactly 64 bytes total. Clean, predictable boundary alignment.
typedef struct {
    char name[MAX_FILENAME]; // 47 bytes
    uint8_t  is_directory;   // 1 byte  (1 = Folder, 0 = File)
    uint8_t  is_used;        // 1 byte  (1 = Active file slot, 0 = Empty slot)
    uint32_t start_sector;   // 4 bytes (Physical block offset on disk)
    uint32_t size_bytes;     // 4 bytes (Actual file payload data boundary)
    uint8_t  padding[7];     // 7 bytes (Pads structural footprint out to 64 bytes)
} __attribute__((packed)) dict_entry_t;

void init_customfs(void);
void customfs_list_files(void);
int  customfs_create_file(char* name, uint32_t initial_sectors);
int  customfs_create_dir(char* name);

#endif
