#include <stdint.h>

// Low-level port I/O helpers
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// PIT hardware port definitions
#define PIT_CHANNEL0_PORT 0x40
#define PIT_COMMAND_PORT  0x43
#define PIT_BASE_FREQ     1193180

void pit_init(uint32_t frequency) {
    if (frequency == 0) {
        return;
    }

    // Calculate the divisor based on the PIT base clock (1.19318 MHz)
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    if (divisor > 65535) {
        divisor = 65535; // Maximum 16-bit divisor limit
    }

    // Send the command byte:
    // Bits 6-7: Select Channel 0 (00)
    // Bits 4-5: Access mode lobyte/hibyte (11)
    // Bits 1-3: Mode 3 - Square Wave Generator (011)
    // Bit 0: Binary mode 16-bit (0)
    // Combined hex value: 0x36
    outb(PIT_COMMAND_PORT, 0x36);

    // Send the frequency divisor (low byte first, then high byte)
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));
}