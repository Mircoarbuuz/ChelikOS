#ifndef TERMINAL_H
#define TERMINAL_H
#include "gohufont.h"
#include "../int.h"
#include "../def.h"
#include "../../libc/textmode/vga.h"
#include "../../libc/string.h"
#include "../inb.h"

#define INPUT_BUFFER_SIZE 256

static uint16_t* terminal_fb = 0; 
static uint32_t fb_width = 1024;
static uint32_t fb_height = 768;
static uint32_t fb_pitch_pixels = 1024; 
static uint8_t  fb_bpp = 16;

static size_t term_cols = 113; // 1024 / 9 = ~113 columns
static size_t term_rows = 54;  // 768 / 14 = ~54 rows for Gohufont 14px

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;

void terminal_init_from_multiboot(uint32_t* mbi_addr) {
    if (mbi_addr) {
        uint32_t total_size = mbi_addr[0];
        uint32_t tag_offset = 8;
        
        while (tag_offset < total_size) {
            uint32_t* tag = (uint32_t*)((char*)mbi_addr + tag_offset);
            uint32_t type = tag[0];
            uint32_t size = tag[1];
            
            if (type == 0) break; 
            
            if (type == 8) {
                uint64_t addr = *(uint64_t*)((char*)tag + 8);
                uint32_t pitch_bytes = *(uint32_t*)((char*)tag + 16);
                uint32_t width = *(uint32_t*)((char*)tag + 20);
                uint32_t height = *(uint32_t*)((char*)tag + 24);
                uint8_t  bpp = *(uint8_t*)((char*)tag + 28);

                terminal_fb = (uint16_t*)(uint64_t)addr;
                fb_width = width;
                fb_height = height;
                fb_pitch_pixels = pitch_bytes / 2; 
                fb_bpp = bpp;

                term_cols = width / 9; 
                term_rows = height / 14;
                break;
            }
            
            tag_offset += size;
            tag_offset = (tag_offset + 7) & ~7;
        }
    }

    if (!terminal_fb) {
        terminal_fb = (uint16_t*)0xFD000000;
        fb_width = 1024;
        fb_height = 768;
        fb_pitch_pixels = 1024;
        fb_bpp = 16;
        term_cols = 113;
        term_rows = 54;
    }
}

void terminal_draw_char(size_t cx, size_t cy, char c, uint8_t color) {
    if (!terminal_fb) return;

    int px = cx * 9; 
    int py = cy * 14; 
    unsigned char uc = (unsigned char)c;

    if (uc >= 'a' && uc <= 'z') {
        uc = uc - 'a' + 'A';
    }

    uint16_t fg_color = 0xFFFF;
    switch (color & 0x0F) {
        case 0:  fg_color = 0x0000; break;
        case 1:  fg_color = 0x001F; break;
        case 2:  fg_color = 0x07E0; break;
        case 3:  fg_color = 0x07FF; break;
        case 4:  fg_color = 0xF800; break;
        case 5:  fg_color = 0xF81F; break;
        case 6:  fg_color = 0xFD20; break;
        case 7:  fg_color = 0xC618; break;
        case 8:  fg_color = 0x8410; break;
        case 9:  fg_color = 0x841F; break;
        case 10: fg_color = 0x07FF; break;
        case 11: fg_color = 0x87FF; break;
        case 12: fg_color = 0xF810; break;
        case 13: fg_color = 0xF81F; break;
        case 14: fg_color = 0xFFE0; break;
        case 15: fg_color = 0xFFFF; break;
    }

    for (int y = 0; y < 14; y++) {
        uint8_t line = gohufont_8x14[uc][y];
        for (int x = 0; x < 8; x++) {
            if (line & (1 << (7 - x))) {
                int target_x = px + x;
                int target_y = py + y;
                if (target_x < (int)fb_width && target_y < (int)fb_height) {
                    terminal_fb[target_y * fb_pitch_pixels + target_x] = fg_color;
                }
            }
        }
    }
}

// Clears a full 9x14 cell with solid black
void terminal_clear_cell(size_t cx, size_t cy) {
    if (!terminal_fb) return;
    int px = cx * 9;
    int py = cy * 14;
    for (int y = 0; y < 14; y++) {
        for (int x = 0; x < 9; x++) {
            int target_x = px + x;
            int target_y = py + y;
            if (target_x < (int)fb_width && target_y < (int)fb_height) {
                terminal_fb[target_y * fb_pitch_pixels + target_x] = 0x0000;
            }
        }
    }
}

void terminal_initialize(void) 
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    if (terminal_fb) {
        for (size_t y = 0; y < fb_height; y++) {
            for (size_t x = 0; x < fb_width; x++) {
                terminal_fb[y * fb_pitch_pixels + x] = 0x0000;
            }
        }
    }
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) { terminal_draw_char(x, y, c, color); }

void terminal_scroll(void) {
    if (!terminal_fb) return;

    size_t row_words = 14 * fb_pitch_pixels;
    size_t total_words = fb_height * fb_pitch_pixels;
    
    for (size_t i = row_words; i < total_words; i++) {
        terminal_fb[i - row_words] = terminal_fb[i];
    }

    for (size_t i = total_words - row_words; i < total_words; i++) {
        terminal_fb[i] = 0x0000;
    }
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == term_rows) {
            terminal_scroll();
            terminal_row = term_rows - 1;
        }
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == term_cols) {
        terminal_column = 0;
        if (++terminal_row == term_rows) {
            terminal_scroll();
            terminal_row = term_rows - 1;
        }
    }
}

void terminal_putchar_c(char c, uint8_t col) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == term_rows) {
            terminal_scroll();
            terminal_row = term_rows - 1;
        }
        return;
    }

    terminal_putentryat(c, col, terminal_column, terminal_row);
    if (++terminal_column == term_cols) {
        terminal_column = 0;
        if (++terminal_row == term_rows) {
            terminal_scroll();
            terminal_row = term_rows - 1;
        }
    }
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_write_c(const char* data, size_t size, uint8_t color) {
    for (size_t i = 0; i < size; i++) terminal_putchar_c(data[i], color);
}

void terminal_writestring(const char* data) { terminal_write(data, strlen(data)); }
void terminal_writestring_c(const char* data, uint8_t col) { terminal_write_c(data, strlen(data), col); }

static char keyboard_read_char(void) {
    static int shift_pressed = 0;
    while (!(inb(0x64) & 0x01));
    uint8_t scancode = inb(0x60);
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return 0; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return 0; }
    if (scancode & 0x80) return 0;

    static const char ascii_table[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
       '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };
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
            return;
        } else if (c == '\b') {
            if (index > 0) {
                index--;
                if (terminal_column == 0) {
                    if (terminal_row > 0) {
                        terminal_row--;
                        terminal_column = term_cols - 1;
                    }
                } else {
                    terminal_column--;
                }
                // Draw solid 9x14 black square to cleanly erase character
                terminal_clear_cell(terminal_column, terminal_row);
            }
        } else if (index < max_len - 1 && c >= 32 && c <= 126) {
            buffer[index++] = c;
            terminal_putchar(c);
        }
    }
}

#endif // TERMINAL_H