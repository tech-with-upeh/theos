#include "ata.h"
#include "util.h"
#include "kmalloc.h"
#include "stdlib/stdio.h"

static vfs_node_t* ata_device_node = NULL;

// New Helper: Creates the mandatory 400ns bus delay required by the ATA specification
static void ata_io_delay(void) {
    inPortB(0x3F6);
    inPortB(0x3F6);
    inPortB(0x3F6);
    inPortB(0x3F6);
}

// Poll the status register until the busy flag (BSY) is cleared
static void ata_wait_ready(void) {
    ata_io_delay();
    while (inPortB(ATA_STATUS) & ATA_STATUS_BSY);
}

// Poll until the data request flag (DRQ) is explicitly set by the controller
static void ata_wait_drq(void) {
    ata_io_delay();
    while (!(inPortB(ATA_STATUS) & ATA_STATUS_DRQ));
}

// Read a single 512-byte sector using 28-bit LBA addressing
void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_ready();

    outPortB(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_delay();
    
    outPortB(ATA_SECTOR_CNT, 1);
    outPortB(ATA_LBA_LOW,  (uint8_t)lba);
    outPortB(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outPortB(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    
    outPortB(ATA_COMMAND, ATA_CMD_READ);
    
    // Wait for the drive to process command and signal data availability
    ata_wait_ready();
    ata_wait_drq();

    uint16_t* buf16 = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        uint8_t low = inPortB(ATA_DATA);
        uint8_t high = inPortB(ATA_DATA);
        buf16[i] = low | (high << 8);
    }
}

// Write a single 512-byte sector using 28-bit LBA addressing
void ata_write_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_ready();

    outPortB(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_delay();
    
    outPortB(ATA_SECTOR_CNT, 1);
    outPortB(ATA_LBA_LOW,  (uint8_t)lba);
    outPortB(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outPortB(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    
    outPortB(ATA_COMMAND, ATA_CMD_WRITE);
    
    // CRITICAL: Must wait for the drive cache to open up *after* the command is sent
    ata_wait_ready();
    ata_wait_drq();

    uint16_t* buf16 = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outPortB(ATA_DATA, (uint8_t)(buf16[i] & 0xFF));
        outPortB(ATA_DATA, (uint8_t)(buf16[i] >> 8));
    }
    
    // Flush cache to commit data to persistent storage media
    outPortB(ATA_COMMAND, 0xE7);
    ata_wait_ready();
}

uint32_t ata_vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t target_sector = offset / 512;
    uint8_t* sector_scratch = (uint8_t*)kmalloc(512);
    
    ata_read_sector(target_sector, sector_scratch);
    
    uint32_t sector_offset = offset % 512;
    memcpy(buffer, sector_scratch + sector_offset, size);
    return size;
}

uint32_t ata_vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t target_sector = offset / 512;
    uint32_t sector_offset = offset % 512;
    uint8_t* sector_scratch = (uint8_t*)kmalloc(512);

    if (size < 512) {
        ata_read_sector(target_sector, sector_scratch);
    }

    memcpy(sector_scratch + sector_offset, buffer, size);
    ata_write_sector(target_sector, sector_scratch);
    return size;
}

void init_ata(void) {
    printf("Initializing ATA Hard Disk Driver...\n");
    
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
