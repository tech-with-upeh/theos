#ifndef ATA_H
#define ATA_H

#include "stdint.h"
#include "vfs.h"

// Standard Legacy I/O Ports for Primary ATA Bus
#define ATA_DATA        0x1F0
#define ATA_FEATURES    0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_SEL   0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

// Essential ATA Commands
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30

// Status Register Bit Masks
#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRQ  0x08

// Kernel Setup Functions
void init_ata(void);
vfs_node_t* ata_get_vfs_node(void);
uint32_t ata_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);

#endif
