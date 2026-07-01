#include "ata.h"
#include "util.h"
#include "kmalloc.h"
#include "stdlib/stdio.h"

uint16_t ata_base_io = 0x1F0; 
uint16_t ata_base_ctrl = 0x3F6; // Dynamically adjusted along with base IO
static vfs_node_t* ata_device_node = NULL;

// 1. Spec-Compliant 400ns Hardware Bus Delays
static void ata_io_delay(void) {
    // Read the alternate status port mapped to this controller 4 times
    inPortB(ata_base_ctrl);
    inPortB(ata_base_ctrl);
    inPortB(ata_base_ctrl);
    inPortB(ata_base_ctrl);
}

static void ata_wait_ready(void) {
    ata_io_delay();
    uint8_t status;
    while (1) {
        status = inPortB(ATA_REG_STATUS);
        if (status == 0xFF) {
            printf("ATA Error: Floating bus detected! No drive connected.\n");
            return; 
        }
        if (!(status & ATA_STATUS_BSY)) {
            break;
        }
    }
}

static void ata_wait_drq(void) {
    ata_io_delay();
    uint8_t status;
    while (1) {
        status = inPortB(ATA_REG_STATUS);
        if (status == 0xFF) return;
        if (status & ATA_STATUS_DRQ) {
            break;
        }
    }
}


// 3. Robust 16-Bit PIO Sector Reader
void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_ready();

    outPortB(ATA_REG_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_delay();
    
    outPortB(ATA_REG_SECTOR_CNT, 1);
    outPortB(ATA_REG_LBA_LOW,  (uint8_t)lba);
    outPortB(ATA_REG_LBA_MID,  (uint8_t)(lba >> 8));
    outPortB(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    
    outPortB(ATA_REG_COMMAND, ATA_CMD_READ);
    
    ata_wait_ready();
    ata_wait_drq();

    // CORRECT: Read 256 words using 16-bit IN instructions
    uint16_t* buf16 = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        buf16[i] = inPortL(ATA_REG_DATA); // Must be a 16-bit reading primitive
    }
}

// 4. Robust 16-Bit PIO Sector Writer
void ata_write_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_ready();

    outPortB(ATA_REG_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_delay();
    
    outPortB(ATA_REG_SECTOR_CNT, 1);
    outPortB(ATA_REG_LBA_LOW,  (uint8_t)lba);
    outPortB(ATA_REG_LBA_MID,  (uint8_t)(lba >> 8));
    outPortB(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    
    outPortB(ATA_REG_COMMAND, ATA_CMD_WRITE);
    
    ata_wait_ready();
    ata_wait_drq();

    // CORRECT: Stream 256 words using 16-bit OUT instructions
    uint16_t* buf16 = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outPortL(ATA_REG_DATA, buf16[i]); // Must be a 16-bit writing primitive
    }
    
    outPortB(ATA_REG_COMMAND, 0xE7); // Cache Flush
    ata_wait_ready();
}

// 5. Complete, Boundary-Safe VFS Read Integration
uint32_t ata_vfs_read(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t bytes_read = 0;
    uint8_t* sector_scratch = (uint8_t*)kmalloc(512);

    while (bytes_read < size) {
        uint32_t current_offset = offset + bytes_read;
        uint32_t target_sector = current_offset / 512;
        uint32_t sector_offset = current_offset % 512;
        
        ata_read_sector(target_sector, sector_scratch);
        
        uint32_t chunk_size = 512 - sector_offset;
        if (chunk_size > (size - bytes_read)) {
            chunk_size = size - bytes_read;
        }

        memcpy(buffer + bytes_read, sector_scratch + sector_offset, chunk_size);
        bytes_read += chunk_size;
    }

    // kfree(sector_scratch);
    return bytes_read;
}

// 6. Complete, Boundary-Safe VFS Write Integration
uint32_t ata_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t bytes_written = 0;
    uint8_t* sector_scratch = (uint8_t*)kmalloc(512);

    while (bytes_written < size) {
        uint32_t current_offset = offset + bytes_written;
        uint32_t target_sector = current_offset / 512;
        uint32_t sector_offset = current_offset % 512;
        
        uint32_t chunk_size = 512 - sector_offset;
        if (chunk_size > (size - bytes_written)) {
            chunk_size = size - bytes_written;
        }

        // If performing partial sector writes, read modified baseline state first
        if (chunk_size < 512) {
            ata_read_sector(target_sector, sector_scratch);
        }

        memcpy(sector_scratch + sector_offset, buffer + bytes_written, chunk_size);
        ata_write_sector(target_sector, sector_scratch);
        
        bytes_written += chunk_size;
    }

    // kfree(sector_scratch);
    return bytes_written;
}

// 7. Dynamic Initializer 
void init_ata(uint16_t dynamic_io_port) {
    ata_base_io = dynamic_io_port;
    
    // Auto-calculate matching legacy Control registers 
    if (ata_base_io == 0x1F0) ata_base_ctrl = 0x3F6;
    else if (ata_base_io == 0x170) ata_base_ctrl = 0x376;
    else ata_base_ctrl = dynamic_io_port + 0x206; // Standard PCI offset projection fallback

    printf("ATA Controller initializing on dynamic hardware port: 0x%x (Ctrl: 0x%x)\n", ata_base_io, ata_base_ctrl);
    
    ata_device_node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(ata_device_node, 0, sizeof(vfs_node_t));
    
    ata_device_node->name = "hda";
    ata_device_node->type = VFS_FILE;
    ata_device_node->size = 1024 * 1024 * 10; 
    
    ata_device_node->read  = ata_vfs_read;     
    ata_device_node->write = ata_vfs_write; 
}

vfs_node_t* ata_get_vfs_node(void) {
    return ata_device_node;
}
