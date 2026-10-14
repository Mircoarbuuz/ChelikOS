#include "../int.h"
#include "../def.h"
#include "../../libc/textmode/vga.h"
#include "../../libc/string.h"
#include "../inb.h"

#define INPUT_BUFFER_SIZE 256
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000 

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

void terminal_initialize(void) 
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_setcolor(uint8_t color) 
{
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_scroll(void) 
{
    // Copy each row to the row above it
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t prev_index = (y - 1) * VGA_WIDTH + x;
            const size_t curr_index = y * VGA_WIDTH + x;
            terminal_buffer[prev_index] = terminal_buffer[curr_index];
        }
    }

    // Clear the bottom row
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
}

void terminal_putchar(char c) 
{
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    }
}
void terminal_putchar_c(char c, uint8_t col) 
{
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
        return;
    }

    // Use the passed 'col' instead of global terminal_color
    terminal_putentryat(c, col, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    }
}
void terminal_write(const char* data, size_t size) 
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}
void terminal_write_c(const char* data, size_t size, uint8_t color) 
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar_c(data[i], color); // <-- Passed 'color' here
}
void terminal_writestring(const char* data) 
{
    terminal_write(data, strlen(data));
}
void terminal_writestring_c(const char* data, uint8_t col) 
{
    terminal_write_c(data, strlen(data), col); // <-- Passed 'col' here
}
static char keyboard_read_char(void) {
    static int shift_pressed = 0;

    // Wait until the keyboard controller has data
    while (!(inb(0x64) & 0x01));
    
    uint8_t scancode = inb(0x60);
    
    // Track Left Shift (0x2A make / 0xAA break) and Right Shift (0x36 make / 0xB6 break)
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return 0;
    }

    // Ignore other key releases (break codes have the high bit set: 0x80)
    if (scancode & 0x80) return 0;

    // Standard US QWERTY lowercase table
    static const char ascii_table[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
       '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };

    // Shifted US QWERTY table
    static const char shift_ascii_table[128] = {
        0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
       '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0, '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        '*', 0, ' '
    };

    if (scancode < 128) {
        return shift_pressed ? shift_ascii_table[scancode] : ascii_table[scancode];
    }
    return 0;
}

void terminal_getkey(const char* prompt, char* buffer, size_t max_len) {
    terminal_writestring(prompt);
    size_t index = 0;

    while (1) {
        char c = keyboard_read_char();
        if (c == 0) continue;

        if (c == '\n') {
            buffer[index] = '\0';
            terminal_putchar('\n');
            return; // Returns control back to kernel_main
        } 
        else if (c == '\b') {
            if (index > 0) {
                index--;
                if (terminal_column == 0) {
                    if (terminal_row > 0) {
                        terminal_row--;
                        terminal_column = VGA_WIDTH - 1;
                    }
                } else {
                    terminal_column--;
                }
                terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
            }
        } 
        else if (index < max_len - 1 && c >= 32 && c <= 126) {
            buffer[index++] = c;
            terminal_putchar(c);
        }
    }
}