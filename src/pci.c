#include "pci.h"
#include "util.h"
#include "stdlib/stdio.h" // Assumes printf exists here per your kernel.c includes

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    // Construct the 32-bit configuration address
    uint32_t address = (uint32_t)(
        ((uint32_t)bus << 16)  |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8)  |
        (offset & 0xFC)        |
        ((uint32_t)0x80000000)  // Enable Bit (Bit 31 must be 1)
    );

    // Send target descriptor out to Configuration Address Space
    outPortL(0xCF8, address);

    // Pull down the matching properties from Configuration Data Space
    return inPortL(0xCFC);
}

void pci_scan_bus(void) {
    printf("\n--- SCANNINNG PCI HARDWARE BUS ---\n");
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            // Read 32-bits containing Vendor ID (bits 0-15) & Device ID (bits 16-31)
            uint32_t reg0 = pci_read_config((uint8_t)bus, slot, 0, 0);
            uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
            uint16_t device_id = (uint16_t)(reg0 >> 16);

            // 0xFFFF indicates no responsive device mapped to this bus configuration slot
            if (vendor_id != 0xFFFF) {
                // Read Class Code information to categorize the hardware type
                uint32_t reg8 = pci_read_config((uint8_t)bus, slot, 0, 0x08);
                uint8_t class_code = (uint8_t)(reg8 >> 24);
                uint8_t subclass   = (uint8_t)(reg8 >> 16);

                printf("PCI [%d:%d:0] -> Vendor:0x%x Device:0x%x Class:0x%x Subclass:0x%x\n", 
                       bus, slot, vendor_id, device_id, class_code, subclass);
                
                // --- STRATEGIC INTERCEPT HOOK FOR FUTURE STORAGE CONTROLLERS ---
                if (class_code == 0x01) { // Storage Controller Group
                    if (subclass == 0x01) {
                        printf(" -> Found Match: Legacy IDE Controller!\n");
                    } else if (subclass == 0x06) {
                        printf(" -> Found Match: AHCI SATA Controller!\n");
                    }
                }
            }
        }
    }
    printf("--- PCI HARDWARE SCAN COMPLETE ---\n\n");
}
