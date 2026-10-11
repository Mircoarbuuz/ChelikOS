#include <stdint.h>

#define ATA_PRIMARY_PORT_DATA       0x1F0
#define ATA_PRIMARY_PORT_SECCOUNT   0x1F2
#define ATA_PRIMARY_PORT_LBA_LOW    0x1F3
#define ATA_PRIMARY_PORT_LBA_MID    0x1F4
#define ATA_PRIMARY_PORT_LBA_HIGH   0x1F5
#define ATA_PRIMARY_PORT_DEVICE     0x1F6
#define ATA_PRIMARY_PORT_COMMAND    0x1F7

#define ATA_CMD_READ_PIO            0x20
#define ATA_CMD_WRITE_PIO           0x30

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void insw(uint16_t port, void *addr, uint32_t word_count) {
    __asm__ volatile ("cld; rep insw" : "+D"(addr), "+c"(word_count) : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void *addr, uint32_t word_count) {
    __asm__ volatile ("cld; rep outsw" : "+S"(addr), "+c"(word_count) : "d"(port) : "memory");
}

int disk_read_sectors(uint32_t lba, uint32_t count, uint8_t *buffer) {
    if (count != 1) return -1;

    // 1. Wait until the drive is not busy (BSY bit = 0)
    int timeout = 1000000;
    while ((inb(ATA_PRIMARY_PORT_COMMAND) & 0x80) && --timeout > 0);
    if (timeout <= 0) return -1;

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
        
        if (status & 0x01) return -2; // ATA Error bit set
        if (status & 0x20) return -3; // Drive Fault bit set

        if (!(status & 0x80) && (status & 0x08)) {
            break; // Ready to read data
        }

        if (--timeout <= 0) return -4;
    }

    // 5. Transfer the 512 bytes (256 words) from data port
    insw(ATA_PRIMARY_PORT_DATA, buffer, 256);

    return 0;
}

int disk_write_sectors(uint32_t lba, uint32_t count, const uint8_t *buffer) {
    if (count != 1) return -1;

    // 1. Wait until the drive is not busy
    int timeout = 1000000;
    while ((inb(ATA_PRIMARY_PORT_COMMAND) & 0x80) && --timeout > 0);
    if (timeout <= 0) return -1;

    // 2. Select drive and send 28-bit LBA parameters
    outb(ATA_PRIMARY_PORT_DEVICE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_PORT_SECCOUNT, (uint8_t)count);
    outb(ATA_PRIMARY_PORT_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_PORT_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // 3. Issue the Write PIO command
    outb(ATA_PRIMARY_PORT_COMMAND, ATA_CMD_WRITE_PIO);

    // 4. Poll for DRQ readiness
    timeout = 1000000;
    while (1) {
        uint8_t status = inb(ATA_PRIMARY_PORT_COMMAND);
        
        if (status & 0x01) return -2;
        if (status & 0x20) return -3;

        if (!(status & 0x80) && (status & 0x08)) {
            break; // Ready to write data
        }

        if (--timeout <= 0) return -4;
    }

    // 5. Transfer the 512 bytes (256 words) to data port
    outsw(ATA_PRIMARY_PORT_DATA, buffer, 256);

    // 6. Flush cache command (optional but recommended for safety)
    outb(ATA_PRIMARY_PORT_COMMAND, 0xE7);
    timeout = 1000000;
    while ((inb(ATA_PRIMARY_PORT_COMMAND) & 0x80) && --timeout > 0);

    return 0;
}

// Wrapper functions expected by fat.c
int disk_read_sector(uint32_t sector, uint8_t *buffer) {
    return disk_read_sectors(sector, 1, buffer);
}

int disk_write_sector(uint32_t sector, const uint8_t *buffer) {
    return disk_write_sectors(sector, 1, buffer);
}