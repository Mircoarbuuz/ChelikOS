#include "libc/textmode/terminal.h"
#include "libc/fs/fat.h"
#include "libc/int.h"
#include "libc/ico.h"
void print_file(const char *name, uint8_t attributes) {
    terminal_writestring(" ");
    terminal_writestring(name);
    
    // Check if it's a directory using the FAT attribute byte
    if ((attributes & 0x10) != 0) {
        terminal_writestring(" [DIR]");
    }
    
    terminal_writestring("\n");
}
void kernel_main(void) {
    terminal_initialize();
    terminal_writestring("ChelikOS 0.011\n");

    fat32_filesystem_t fs;
    if (fat32_mount(0, &fs) != 0) {
        terminal_writestring("Mount failed!\n");
        return;
    }

    terminal_writestring("FAT32 mounted successfully!\n\n");

    char input_buffer[256];

    while (1) {
        // Read a line (blocks until enter, just like fgets)
        terminal_getkey("chelik ] ", input_buffer, sizeof(input_buffer));

        // Evaluate commands directly
        if (strcmp(input_buffer, "ls") == 0 || strcmp(input_buffer, "dir") == 0) {
            fat32_list_root_dir(&fs, print_file);
        } 
        else if (strcmp(input_buffer, "clear") == 0) {
            terminal_initialize();
        } 
        else if (strcmp(input_buffer, "logo") == 0) {
            terminal_writestring(logo);
        }
        else if (strcmp(input_buffer, "help") == 0) {
            terminal_writestring("logo, ls, clear, help\n");
        } 
        else if (input_buffer[0] != '\0') {
            terminal_writestring("Unknown command: ");
            terminal_writestring(input_buffer);
            terminal_writestring("\n");
        }
    }
}