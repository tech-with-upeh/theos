#include "pci.h"
#include "util.h"
#include "stdlib/stdio.h" 
#include "ata.h"

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(
        ((uint32_t)bus << 16)  |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8)  |
        (offset & 0xFC)        |
        ((uint32_t)0x80000000)  
    );
    outPortL(0xCF8, address);
    return inPortL(0xCFC);
}

// Extracted logic into a helper function to cleanly handle multi-function devices
void check_pci_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t reg0 = pci_read_config(bus, slot, func, 0x00);
    uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
    uint16_t device_id = (uint16_t)(reg0 >> 16);

    if (vendor_id == 0xFFFF) return;

    uint32_t reg8 = pci_read_config(bus, slot, func, 0x08);
    uint8_t class_code = (uint8_t)(reg8 >> 24);
    uint8_t subclass   = (uint8_t)(reg8 >> 16);

    printf("PCI [%d:%d:%d] -> Vendor:0x%x Device:0x%x Class:0x%x Subclass:0x%x\n", 
           bus, slot, func, vendor_id, device_id, class_code, subclass);
    
    if (class_code == 0x01) { // Storage Controller Group
    if (subclass == 0x01) {
        printf(" -> Found Match: Legacy IDE Controller!\n");
        
        // Force standard Primary Legacy PIO ports for early OS development
        uint16_t assigned_port = 0x1F0; 
        
        printf(" -> Hardware Port mapped via Legacy Bypass: 0x%x\n", assigned_port);
        init_ata(assigned_port);
    }
}

}

void pci_scan_bus(void) {
    printf("\n--- SCANNINNG PCI HARDWARE BUS ---\n");
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            // Test function 0 first
            uint32_t reg0 = pci_read_config((uint8_t)bus, slot, 0, 0);
            uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);

            if (vendor_id != 0xFFFF) {
                // Read cache line size, lat timer, header type, BIST register (Offset 0x0C)
                uint32_t regC = pci_read_config((uint8_t)bus, slot, 0, 0x0C);
                uint8_t header_type = (uint8_t)(regC >> 16);

                // Always check function 0
                check_pci_function((uint8_t)bus, slot, 0);

                // If bit 7 of header_type is set, this slot houses a multi-function device
                if (header_type & 0x80) {
                    for (uint8_t func = 1; func < 8; func++) {
                        check_pci_function((uint8_t)bus, slot, func);
                    }
                }
            }
        }
    }
    printf("--- PCI HARDWARE SCAN COMPLETE ---\n\n");
}
