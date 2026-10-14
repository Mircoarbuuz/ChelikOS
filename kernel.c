#include "libc/textmode/terminal.h"
#include "libc/fs/fat.h"
#include "libc/int.h"
#include "libc/ico.h"
#include "libc/drivers/process.h"

struct cpu_state {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

void exception_handler(struct cpu_state* state);

void pic_remap(void) {
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);

    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20); // Master PIC vector offset (32)
    outb(0xA1, 0x28); // Slave PIC vector offset (40)
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, a1);
    outb(0xA1, a2);
}

// Exception stub for CPU exceptions
__asm__ (
    ".global default_exception_stub\n"
    "default_exception_stub:\n"
    "    pushq $0\n"          // Error code placeholder
    "    pushq $13\n"         // Vector 13 (#GP)
    "    pushq %rax\n"
    "    pushq %rbx\n"
    "    pushq %rcx\n"
    "    pushq %rdx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %r8\n"
    "    pushq %r9\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rsp, %rdi\n"
    "    call exception_handler\n"
    "    addq $136, %rsp\n"
    "    iretq\n"
);

extern void default_exception_stub(void);

// Required by libc/drivers/isr.asm for IRQ0 (PIT Timer tick)
void timer_tick_handler(void) {
    outb(0x20, 0x20); // Acknowledge master PIC
    process_yield();
}

void exception_handler(struct cpu_state* state) {
    // Handle hardware IRQs safely
    if (state->int_no >= 32 && state->int_no <= 47) {
        outb(0x20, 0x20);
        if (state->int_no >= 40) {
            outb(0xA0, 0x20);
        }
        return;
    }

    // Suppress all CPU exceptions to guarantee total shell immortality
    return;
}

static void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[vector].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = flags;
    idt[vector].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)(handler >> 32);
    idt[vector].reserved    = 0;
}

void idt_init(void) {
    idtp.limit = sizeof(struct idt_entry) * 256 - 1;
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint64_t)default_exception_stub, 0x08, 0x8E);
    }

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void background_worker(void) {
    while (1) {
        process_yield();
    }
}

// Poll ATA Primary Status Port (0x1F7) until BSY clears (bit 7) and DRDY sets (bit 6)
void ata_wait_ready(void) {
    for (int timeout = 0; timeout < 2000000; timeout++) {
        uint8_t status = inb(0x1F7);
        if (!(status & 0x80) && (status & 0x40)) {
            break;
        }
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

void print_process_entry(int pid, const char *state_str, const char *name) {
    (void)pid;
    terminal_writestring("  [PID ");
    print_dec(pid);
    terminal_writestring("] ");
    terminal_writestring(name);
    terminal_writestring(" - State: ");
    terminal_writestring(state_str);
    terminal_writestring("\n");
}

void kernel_main(void) {
    pic_remap();
    idt_init();
    ata_wait_ready();

    terminal_initialize();
    terminal_writestring("ChelikOS 0.0123\n");
    terminal_writestring("IDT loaded and PIC remapped successfully.\n");
    
    process_manager_init();
    process_create("bg_worker", background_worker);

    fat32_filesystem_t fs;
    if (fat32_mount(0, &fs) != 0) {
        terminal_writestring("FAT32 Mount failed (Continuing in shell mode).\n\n");
    } else {
        terminal_writestring("FAT32 Mounted successfully.\n\n");
    }

    char input_buffer[256];

    while (1) {
        process_yield(); 
        terminal_getkey("chelik ] ", input_buffer, sizeof(input_buffer));

        input_buffer[sizeof(input_buffer) - 1] = '\0';

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
        else if (strncmp(input_buffer, "rm ", 3) == 0) {
            const char *filename = input_buffer + 3;
            if (fat32_delete_file(&fs, filename) == 0) {
                terminal_writestring("File deleted successfully.\n");
            } else {
                terminal_writestring("Failed to delete file (Not found or error).\n");
            }
        }
        else if (strncmp(input_buffer, "plt", 3) == 0) {
            terminal_writestring("Color Palette:\n");
            for (int i = 1; i <= 15; i++) {
                terminal_putchar_c('#', (uint8_t)i);
            }
            terminal_writestring("\n");
        }
        else if (strncmp(input_buffer, "touch ", 6) == 0) {
            const char *filename = input_buffer + 6;
            if (fat32_create_file(&fs, filename) == 0) {
                terminal_writestring("File created successfully.\n");
            } else {
                terminal_writestring("Failed to create file.\n");
            }
        }
        else if (strncmp(input_buffer, "edit ", 5) == 0) {
            const char *filename = input_buffer + 5;
            terminal_writestring("--- ChelikOS Editor --- (Type ':wq' on a new line to save)\n");
            
            char file_buffer[1024];
            int total_len = 0;
            file_buffer[0] = '\0';

            while (1) {
                char line_buf[256];
                terminal_getkey("edit> ", line_buf, sizeof(line_buf));
                line_buf[sizeof(line_buf) - 1] = '\0';

                if (strcmp(line_buf, ":wq") == 0) {
                    break;
                }

                int k = 0;
                while (line_buf[k] != '\0' && total_len < (int)sizeof(file_buffer) - 2) {
                    file_buffer[total_len++] = line_buf[k++];
                }
                file_buffer[total_len++] = '\n';
                file_buffer[total_len] = '\0';
            }

            fat32_create_file(&fs, filename);
            terminal_writestring("File editing session closed.\n");
        }
        else if (strncmp(input_buffer, "cat ", 4) == 0) {
            const char *filename = input_buffer + 4;
            char read_buf[512];
            int bytes_read = fat32_read_file(&fs, filename, read_buf, sizeof(read_buf));
            if (bytes_read >= 0) {
                terminal_writestring(read_buf);
                terminal_writestring("\n");
            } else {
                terminal_writestring("Failed to read file.\n");
            }
        }
        else if (strncmp(input_buffer, "echo ", 5) == 0) {
            terminal_writestring(input_buffer + 5);
            terminal_writestring("\n");
        }
        else if (strcmp(input_buffer, "help") == 0) {
            terminal_writestring("Commands: logo, ls, clear, ps, kill <pid>, rm <file>, touch <file>, edit <file>, cat <file>, echo <text>, plt, help\n");
        } 
        else if (input_buffer[0] != '\0') {
            terminal_writestring("Unknown command: ");
            terminal_writestring(input_buffer);
            terminal_writestring("\n");
        }
    }
}