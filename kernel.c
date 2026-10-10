#include "libc/textmode/terminal.h"
#include "libc/fs/fat.h"
#include "libc/int.h"
#include "libc/ico.h"
#include "libc/drivers/process.h"

void background_worker(void) {
    while (1) {
        // Background loop task yielding execution back to scheduler
        process_yield();
    }
}

void print_file(const char *name, uint8_t attributes) {
    terminal_writestring(" ");
    terminal_writestring(name);
    if ((attributes & 0x10) != 0) {
        terminal_writestring(" [DIR]");
    }
    terminal_writestring("\n");
}

void print_dec(int val) {
    if (val == 0) {
        terminal_writestring("0");
        return;
    }
    char buf[32];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        char s[2] = { buf[--i], '\0' };
        terminal_writestring(s);
    }
}

void print_process_entry(int pid, const char *state, const char *name) {
    (void)pid;
    terminal_writestring("  [PID ");
    print_dec(pid);
    terminal_writestring("] ");
    terminal_writestring(name);
    terminal_writestring(" - State: ");
    terminal_writestring(state);
    terminal_writestring("\n");
}

void kernel_main(void) {
    terminal_initialize();
    terminal_writestring("ChelikOS 0.013 [True Multitasking Active]\n");

    process_manager_init();
    process_create("bg_worker", background_worker);

    fat32_filesystem_t fs;
    if (fat32_mount(0, &fs) != 0) {
        terminal_writestring("Mount failed!\n");
        return;
    }

    terminal_writestring("FAT32 mounted successfully!\n\n");

    char input_buffer[256];

    while (1) {
        process_yield(); // Yield occasionally in the shell loop to let background tasks run
        terminal_getkey("chelik ] ", input_buffer, sizeof(input_buffer));

        if (strcmp(input_buffer, "ls") == 0 || strcmp(input_buffer, "dir") == 0) {
            fat32_list_root_dir(&fs, print_file);
        } 
        else if (strcmp(input_buffer, "clear") == 0) {
            terminal_initialize();
        } 
        else if (strcmp(input_buffer, "logo") == 0) {
            terminal_writestring(logo);
        }
        else if (strcmp(input_buffer, "ps") == 0 || strcmp(input_buffer, "tasks") == 0) {
            terminal_writestring("Active Processes:\n");
            process_list_all(print_process_entry);
        }
        else if (strncmp(input_buffer, "kill ", 5) == 0) {
            int target_pid = 0;
            const char *p = input_buffer + 5;
            while (*p >= '0' && *p <= '9') {
                target_pid = target_pid * 10 + (*p - '0');
                p++;
            }

            if (process_kill(target_pid) == 0) {
                terminal_writestring("Successfully terminated process.\n");
            } else {
                terminal_writestring("Failed to kill process (Invalid PID or protected).\n");
            }
        }
        else if (strcmp(input_buffer, "help") == 0) {
            terminal_writestring("Commands: logo, ls, clear, ps, kill <pid>, help\n");
        } 
        else if (input_buffer[0] != '\0') {
            terminal_writestring("Unknown command: ");
            terminal_writestring(input_buffer);
            terminal_writestring("\n");
        }
    }
}