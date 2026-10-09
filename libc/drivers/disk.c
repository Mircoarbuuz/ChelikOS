#include "../int.h"
#include "../inb.h"
// Low-level x86 port I/O functions(got consumed by inb.h)

#define ATA_PRIMARY_PORT_DATA       0x1F0
#define ATA_PRIMARY_PORT_SECCOUNT   0x1F2
#define ATA_PRIMARY_PORT_LBA_LOW    0x1F3
#define ATA_PRIMARY_PORT_LBA_MID    0x1F4
#define ATA_PRIMARY_PORT_LBA_HIGH   0x1F5
#define ATA_PRIMARY_PORT_DEVICE     0x1F6
#define ATA_PRIMARY_PORT_COMMAND    0x1F7
#define ATA_CMD_READ_PIO            0x20

int disk_read_sectors(uint32_t lba, uint32_t count, uint8_t *buffer) {
    if (count != 1) return -1;

    // 1. Wait until the drive is not busy (BSY bit = 0)
    int timeout = 1000000;
    while ((inb(ATA_PRIMARY_PORT_COMMAND) & 0x80) && --timeout > 0);
    if (timeout <= 0) return -1; // Timeout waiting for drive readiness

    // 2. Select drive and send 28-bit LBA parameters
    outb(ATA_PRIMARY_PORT_DEVICE, 0xE0 | ((lba >> 24) & 0x0F)); // Master drive, LBA mode
    outb(ATA_PRIMARY_PORT_SECCOUNT, (uint8_t)count);
    outb(ATA_PRIMARY_PORT_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_PORT_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // 3. Issue the Read PIO command
    outb(ATA_PRIMARY_PORT_COMMAND, ATA_CMD_READ_PIO);

    // 4. Poll for data request ready (DRQ = 1) and no errors
    timeout = 1000000;
    while (1) {
        uint8_t status = inb(ATA_PRIMARY_PORT_COMMAND);
        
        // Check for error or drive fault bits
        if (status & 0x01) return -2; // ATA Error bit set
        if (status & 0x20) return -3; // Drive Fault bit set

        // Check if BSY is cleared and DRQ is set
        if (!(status & 0x80) && (status & 0x08)) {
            break; // Ready to read data!
        }

        if (--timeout <= 0) return -4; // Timeout waiting for data request
    }

    // 5. Transfer the 512 bytes (256 words) from data port
    insw(ATA_PRIMARY_PORT_DATA, buffer, 256);

    return 0; // Success!
}