#ifndef PCI_H
#define PCI_H

#include "stdint.h"

// Core function declarations
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_scan_bus(void);

#endif
