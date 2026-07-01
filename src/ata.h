#ifndef ATA_H
#define ATA_H

#include "stdint.h"
#include "vfs.h"

// Dynamic Controller Address State Map
extern uint16_t ata_base_io;

// Dynamic port offsets matching standard specifications
#define ATA_REG_DATA        (ata_base_io + 0)
#define ATA_REG_FEATURES    (ata_base_io + 1)
#define ATA_REG_SECTOR_CNT  (ata_base_io + 2)
#define ATA_REG_LBA_LOW     (ata_base_io + 3)
#define ATA_REG_LBA_MID     (ata_base_io + 4)
#define ATA_REG_LBA_HIGH    (ata_base_io + 5)
#define ATA_REG_DRIVE_SEL   (ata_base_io + 6)
#define ATA_REG_COMMAND     (ata_base_io + 7)
#define ATA_REG_STATUS      (ata_base_io + 7)

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRQ  0x08

void init_ata(uint16_t dynamic_io_port);
vfs_node_t* ata_get_vfs_node(void);
void ata_read_sector(uint32_t lba, uint8_t* buffer);
void ata_write_sector(uint32_t lba, uint8_t* buffer);
uint32_t ata_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);

#endif
