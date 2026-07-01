#include "customfs.h"
#include "ata.h"
#include "kmalloc.h"
#include "util.h"
#include "stdlib/stdio.h"

// Forward declaration of internal format routine
static void customfs_format(void);

void init_customfs(void) {
    printf("CustomFS: Reading storage sector 0...\n");

    // Allocate 1 temporary sector scratch buffer on heap
    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    if (!sector_buffer) {
        printf("CustomFS Error: Memory allocation failed during initialization.\n");
        return;
    }

    // Read the absolute first sector from the hard drive
    ata_read_sector(0, sector_buffer);

    // Cast the raw byte chunk directly to our superblock structure layout
    superblock_t* sb = (superblock_t*)sector_buffer;

    printf("CustomFS: Checking magic signature (Found: 0x%x, Expected: 0x%x)\n", 
            sb->magic, CUSTOM_FS_MAGIC);

    if (sb->magic == CUSTOM_FS_MAGIC) {
        printf("CustomFS: Valid filesystem detected!\n");
        printf(" -> Total Disk Sectors: %d\n", sb->total_sectors);
        printf(" -> Active Indexed Files: %d\n", sb->file_count);
    } else {
        printf("CustomFS: Magic mismatch! Drive is raw or corrupted.\n");
        customfs_format();
    }

    // kfree(sector_buffer);
}

static void customfs_format(void) {
    printf("CustomFS: Formatting target drive now...\n");

    uint8_t* format_buffer = (uint8_t*)kmalloc(512);
    if (!format_buffer) return;

    // 1. Prepare clean superblock data structures
    memset(format_buffer, 0, 512);
    superblock_t* sb = (superblock_t*)format_buffer;
    sb->magic = CUSTOM_FS_MAGIC;
    sb->total_sectors = 20480; // 10MB capacity configuration
    sb->file_count = 0;

    // Write superblock right onto Sector 0
    printf("CustomFS: Committing Superblock to hardware block 0...\n");
    ata_write_sector(0, format_buffer);

    // 2. Wipe directory index space (Sectors 1-19) to clear out stale garbage bits
    memset(format_buffer, 0, 512); 
    for (int i = 1; i <= 19; i++) {
        ata_write_sector(i, format_buffer);
    }

    // kfree(format_buffer);
    printf("CustomFS: Format sequence complete. Re-run complete.\n");
}


// --- LIST ALL FILES & FOLDERS ---
void customfs_list_files(void) {
    printf("\n--- CUSTOMFS FILE DIRECTORY LIST ---\n");
    
    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    if (!sector_buffer) return;

    int total_found = 0;

    // Loop through the entire directory indexing workspace (Sectors 1 to 19)
    for (int sector = 1; sector <= 19; sector++) {
        ata_read_sector(sector, sector_buffer);
        dict_entry_t* entries = (dict_entry_t*)sector_buffer;

        // Extract and evaluate all 8 individual 64-byte structural entries per sector block
        for (int entry_idx = 0; entry_idx < 8; entry_idx++) {
            if (entries[entry_idx].is_used == 1) {
                printf("[%s] Type: %s | Size: %d Bytes | Start Sector: %d\n",
                       entries[entry_idx].name,
                       entries[entry_idx].is_directory ? "DIR " : "FILE",
                       entries[entry_idx].size_bytes,
                       entries[entry_idx].start_sector);
                total_found++;
            }
        }
    }

    if (total_found == 0) {
        printf("(Directory is completely empty)\n");
    }
    printf("------------------------------------\n\n");
    // kfree(sector_buffer);
}

// --- CREATE A NEW FILE ---
int customfs_create_file(char* name, uint32_t initial_sectors) {
    printf("CustomFS: Attempting to create file '%s'...\n", name);

    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    if (!sector_buffer) return -1;

    // 1. Scan directory tables to find an empty slot (is_used == 0)
    for (int sector = 1; sector <= 19; sector++) {
        ata_read_sector(sector, sector_buffer);
        dict_entry_t* entries = (dict_entry_t*)sector_buffer;

        for (int entry_idx = 0; entry_idx < 8; entry_idx++) {
            if (entries[entry_idx].is_used == 0) {
                // Found an open slot! Populate it with metadata parameters
                memset(entries[entry_idx].name, 0, MAX_FILENAME);
                
                // Copy characters safely manually to prevent overflow dependencies
                int len = 0;
                while (name[len] != '\0' && len < (MAX_FILENAME - 1)) {
                    entries[entry_idx].name[len] = name[len];
                    len++;
                }
                entries[entry_idx].name[len] = '\0';

                entries[entry_idx].is_directory = 0; // Standard File
                entries[entry_idx].is_used = 1;      // Flag as active
                entries[entry_idx].size_bytes = 0;   // Starts empty

                // 2. Simple Linear Data Block Allocation Scheme:
                // For a dead-simple setup, let's look up how many files exist from the superblock
                // and put the new file right after the previous files.
                // Each file will start at: Sector 20 + (file_count * fixed padding width)
                uint8_t* sb_buffer = (uint8_t*)kmalloc(512);
                ata_read_sector(0, sb_buffer);
                superblock_t* sb = (superblock_t*)sb_buffer;
                
                entries[entry_idx].start_sector = 20 + (sb->file_count * 100); 
                
                // Update file counter bounds inside the master superblock mapping entry
                sb->file_count += 1;
                ata_write_sector(0, sb_buffer);
                // kfree(sb_buffer);

                // 3. Save modified directory sector directly back onto hardware disk
                ata_write_sector(sector, sector_buffer);
                // kfree(sector_buffer);
                
                printf("CustomFS: File '%s' registered successfully at block offset %d!\n", 
                       name, entries[entry_idx].start_sector);
                return 0; // Success
            }
        }
    }

    printf("CustomFS Error: Directory table full! Cannot allocate entry slot.\n");
    // kfree(sector_buffer);
    return -1;
}

int customfs_create_dir(char* name) {
    printf("CustomFS: Attempting to create directory '%s'...\n", name);

    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    if (!sector_buffer) return -1;

    // 1. Scan root directory table (Sectors 1 to 19) to find an empty entry slot
    for (int sector = 1; sector <= 19; sector++) {
        ata_read_sector(sector, sector_buffer);
        dict_entry_t* entries = (dict_entry_t*)sector_buffer;

        for (int entry_idx = 0; entry_idx < 8; entry_idx++) {
            if (entries[entry_idx].is_used == 0) {
                
                // Found an open slot! Set up the entry name
                memset(entries[entry_idx].name, 0, MAX_FILENAME);
                int len = 0;
                while (name[len] != '\0' && len < (MAX_FILENAME - 1)) {
                    entries[entry_idx].name[len] = name[len];
                    len++;
                }
                entries[entry_idx].name[len] = '\0';

                // CRITICAL DIFFERENCE: Mark this entry explicitly as a directory
                entries[entry_idx].is_directory = 1; 
                entries[entry_idx].is_used = 1;      
                entries[entry_idx].size_bytes = 512; // A folder structure baseline is exactly 1 sector size

                // 2. Allocate data block based on total file count
                uint8_t* sb_buffer = (uint8_t*)kmalloc(512);
                ata_read_sector(0, sb_buffer);
                superblock_t* sb = (superblock_t*)sb_buffer;
                
                entries[entry_idx].start_sector = 20 + (sb->file_count * 100); 
                
                sb->file_count += 1;
                ata_write_sector(0, sb_buffer);
                // kfree(sb_buffer);

                // 3. Save modified entry record back to root directory table
                ata_write_sector(sector, sector_buffer);
                
                // 4. PREVENT GARBAGE: Wipe the newly allocated data sector completely clean.
                // This ensures the new sub-directory reads as completely empty, rather than 
                // containing random leftover bytes from old deleted disk contents.
                memset(sector_buffer, 0, 512);
                ata_write_sector(entries[entry_idx].start_sector, sector_buffer);

                // kfree(sector_buffer);
                printf("CustomFS: Directory '%s' registered successfully at block offset %d!\n", 
                       name, entries[entry_idx].start_sector);
                return 0; 
            }
        }
    }

    printf("CustomFS Error: Root directory table full! Cannot allocate entry slot.\n");
    // kfree(sector_buffer);
    return -1;
}

