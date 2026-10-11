#include <stdint.h>

extern void process_yield(void);

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36); // Channel 0, low/high byte, square wave mode
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// Called automatically by the assembly IRQ0 stub every tick
void timer_tick_handler(void) {
    // This forces the CPU to switch tasks automatically!
    process_yield();
}