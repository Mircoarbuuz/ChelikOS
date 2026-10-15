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

void interrupt_handler(struct cpu_state* state);

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

// Safe PIT timer tick: Acknowledge the PIC without forcing dangerous preemption
void timer_tick_handler(void) {
    outb(0x20, 0x20); // Acknowledge master PIC
}

// Unified C interrupt dispatcher
void interrupt_handler(struct cpu_state* state) {
    // Handle Hardware IRQs (Vectors 32-47)
    if (state->int_no >= 32 && state->int_no <= 47) {
        uint8_t irq = (uint8_t)(state->int_no - 32);
        
        outb(0x20, 0x20);
        if (irq >= 8) {
            outb(0xA0, 0x20);
        }

        if (irq == 0) {
            timer_tick_handler();
        }
        return;
    }

    // Handle CPU Exceptions (Vectors 0-31)
    terminal_writestring("\n[CPU EXCEPTION] Vector: ");
    print_dec((int)state->int_no);
    terminal_writestring(" Err: ");
    print_dec((int)state->err_code);
    terminal_writestring("\n");

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

// Macro-generated assembly stubs for all 256 interrupts ensuring correct stack alignment
#define ISR_NO_ERR(num) \
    __asm__ ( \
        ".global isr_stub_" #num "\n" \
        "isr_stub_" #num ":\n" \
        "    pushq $0\n"        /* Dummy error code */ \
        "    pushq $" #num "\n"   /* Vector number */ \
        "    jmp isr_common_stub\n" \
    )

#define ISR_ERR(num) \
    __asm__ ( \
        ".global isr_stub_" #num "\n" \
        "isr_stub_" #num ":\n" \
        "    pushq $" #num "\n"   /* Error code already pushed by CPU */ \
        "    jmp isr_common_stub\n" \
    )

__asm__ (
    ".global isr_common_stub\n"
    "isr_common_stub:\n"
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
    "    call interrupt_handler\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %r9\n"
    "    popq %r8\n"
    "    popq %rbp\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rdx\n"
    "    popq %rcx\n"
    "    popq %rbx\n"
    "    popq %rax\n"
    "    addq $16, %rsp\n" /* Skip int_no and err_code */ \
    "    iretq\n"
);

ISR_NO_ERR(0);  ISR_NO_ERR(1);  ISR_NO_ERR(2);  ISR_NO_ERR(3);
ISR_NO_ERR(4);  ISR_NO_ERR(5);  ISR_NO_ERR(6);  ISR_NO_ERR(7);
ISR_ERR(8);     ISR_NO_ERR(9);  ISR_ERR(10);    ISR_ERR(11);
ISR_ERR(12);    ISR_ERR(13);    ISR_ERR(14);    ISR_NO_ERR(15);
ISR_NO_ERR(16); ISR_ERR(17);    ISR_NO_ERR(18); ISR_NO_ERR(19);
ISR_NO_ERR(20); ISR_ERR(21);    ISR_NO_ERR(22); ISR_NO_ERR(23);
ISR_NO_ERR(24); ISR_NO_ERR(25); ISR_NO_ERR(26); ISR_NO_ERR(27);
ISR_NO_ERR(28); ISR_ERR(29);    ISR_ERR(30);    ISR_NO_ERR(31);

#define IRQ_STUB(i, num) ISR_NO_ERR(num)
IRQ_STUB(0, 32); IRQ_STUB(1, 33); IRQ_STUB(2, 34); IRQ_STUB(3, 35);
IRQ_STUB(4, 36); IRQ_STUB(5, 37); IRQ_STUB(6, 38); IRQ_STUB(7, 39);
IRQ_STUB(8, 40); IRQ_STUB(9, 41); IRQ_STUB(10, 42); IRQ_STUB(11, 43);
IRQ_STUB(12, 44); IRQ_STUB(13, 45); IRQ_STUB(14, 46); IRQ_STUB(15, 47);

extern void* isr_stub_0(void);
extern void* isr_stub_1(void);
extern void* isr_stub_2(void);
extern void* isr_stub_3(void);
extern void* isr_stub_4(void);
extern void* isr_stub_5(void);
extern void* isr_stub_6(void);
extern void* isr_stub_7(void);
extern void* isr_stub_8(void);
extern void* isr_stub_9(void);
extern void* isr_stub_10(void);
extern void* isr_stub_11(void);
extern void* isr_stub_12(void);
extern void* isr_stub_13(void);
extern void* isr_stub_14(void);
extern void* isr_stub_15(void);
extern void* isr_stub_16(void);
extern void* isr_stub_17(void);
extern void* isr_stub_18(void);
extern void* isr_stub_19(void);
extern void* isr_stub_20(void);
extern void* isr_stub_21(void);
extern void* isr_stub_22(void);
extern void* isr_stub_23(void);
extern void* isr_stub_24(void);
extern void* isr_stub_25(void);
extern void* isr_stub_26(void);
extern void* isr_stub_27(void);
extern void* isr_stub_28(void);
extern void* isr_stub_29(void);
extern void* isr_stub_30(void);
extern void* isr_stub_31(void);
extern void* isr_stub_32(void);
extern void* isr_stub_33(void);
extern void* isr_stub_34(void);
extern void* isr_stub_35(void);
extern void* isr_stub_36(void);
extern void* isr_stub_37(void);
extern void* isr_stub_38(void);
extern void* isr_stub_39(void);
extern void* isr_stub_40(void);
extern void* isr_stub_41(void);
extern void* isr_stub_42(void);
extern void* isr_stub_43(void);
extern void* isr_stub_44(void);
extern void* isr_stub_45(void);
extern void* isr_stub_46(void);
extern void* isr_stub_47(void);

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

    idt_set_gate(0, (uint64_t)isr_stub_0, 0x08, 0x8E);
    idt_set_gate(1, (uint64_t)isr_stub_1, 0x08, 0x8E);
    idt_set_gate(2, (uint64_t)isr_stub_2, 0x08, 0x8E);
    idt_set_gate(3, (uint64_t)isr_stub_3, 0x08, 0x8E);
    idt_set_gate(4, (uint64_t)isr_stub_4, 0x08, 0x8E);
    idt_set_gate(5, (uint64_t)isr_stub_5, 0x08, 0x8E);
    idt_set_gate(6, (uint64_t)isr_stub_6, 0x08, 0x8E);
    idt_set_gate(7, (uint64_t)isr_stub_7, 0x08, 0x8E);
    idt_set_gate(8, (uint64_t)isr_stub_8, 0x08, 0x8E);
    idt_set_gate(9, (uint64_t)isr_stub_9, 0x08, 0x8E);
    idt_set_gate(10, (uint64_t)isr_stub_10, 0x08, 0x8E);
    idt_set_gate(11, (uint64_t)isr_stub_11, 0x08, 0x8E);
    idt_set_gate(12, (uint64_t)isr_stub_12, 0x08, 0x8E);
    idt_set_gate(13, (uint64_t)isr_stub_13, 0x08, 0x8E);
    idt_set_gate(14, (uint64_t)isr_stub_14, 0x08, 0x8E);
    idt_set_gate(15, (uint64_t)isr_stub_15, 0x08, 0x8E);
    idt_set_gate(16, (uint64_t)isr_stub_16, 0x08, 0x8E);
    idt_set_gate(17, (uint64_t)isr_stub_17, 0x08, 0x8E);
    idt_set_gate(18, (uint64_t)isr_stub_18, 0x08, 0x8E);
    idt_set_gate(19, (uint64_t)isr_stub_19, 0x08, 0x8E);
    idt_set_gate(20, (uint64_t)isr_stub_20, 0x08, 0x8E);
    idt_set_gate(21, (uint64_t)isr_stub_21, 0x08, 0x8E);
    idt_set_gate(22, (uint64_t)isr_stub_22, 0x08, 0x8E);
    idt_set_gate(23, (uint64_t)isr_stub_23, 0x08, 0x8E);
    idt_set_gate(24, (uint64_t)isr_stub_24, 0x08, 0x8E);
    idt_set_gate(25, (uint64_t)isr_stub_25, 0x08, 0x8E);
    idt_set_gate(26, (uint64_t)isr_stub_26, 0x08, 0x8E);
    idt_set_gate(27, (uint64_t)isr_stub_27, 0x08, 0x8E);
    idt_set_gate(28, (uint64_t)isr_stub_28, 0x08, 0x8E);
    idt_set_gate(29, (uint64_t)isr_stub_29, 0x08, 0x8E);
    idt_set_gate(30, (uint64_t)isr_stub_30, 0x08, 0x8E);
    idt_set_gate(31, (uint64_t)isr_stub_31, 0x08, 0x8E);

    idt_set_gate(32, (uint64_t)isr_stub_32, 0x08, 0x8E);
    idt_set_gate(33, (uint64_t)isr_stub_33, 0x08, 0x8E);
    idt_set_gate(34, (uint64_t)isr_stub_34, 0x08, 0x8E);
    idt_set_gate(35, (uint64_t)isr_stub_35, 0x08, 0x8E);
    idt_set_gate(36, (uint64_t)isr_stub_36, 0x08, 0x8E);
    idt_set_gate(37, (uint64_t)isr_stub_37, 0x08, 0x8E);
    idt_set_gate(38, (uint64_t)isr_stub_38, 0x08, 0x8E);
    idt_set_gate(39, (uint64_t)isr_stub_39, 0x08, 0x8E);
    idt_set_gate(40, (uint64_t)isr_stub_40, 0x08, 0x8E);
    idt_set_gate(41, (uint64_t)isr_stub_41, 0x08, 0x8E);
    idt_set_gate(42, (uint64_t)isr_stub_42, 0x08, 0x8E);
    idt_set_gate(43, (uint64_t)isr_stub_43, 0x08, 0x8E);
    idt_set_gate(44, (uint64_t)isr_stub_44, 0x08, 0x8E);
    idt_set_gate(45, (uint64_t)isr_stub_45, 0x08, 0x8E);
    idt_set_gate(46, (uint64_t)isr_stub_46, 0x08, 0x8E);
    idt_set_gate(47, (uint64_t)isr_stub_47, 0x08, 0x8E);

    for (int i = 48; i < 256; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_31, 0x08, 0x8E);
    }

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void background_worker(void) {
    while (1) {
        process_yield();
    }
}

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

void kernel_main(uint32_t magic, uint32_t addr) {
    if (magic == 0x36d76289) {
        terminal_init_from_multiboot((uint32_t*)(uint64_t)addr);
    }
    
    pic_remap();
    idt_init();
    ata_wait_ready();

    terminal_initialize();
    terminal_writestring("ChelikOS 0.0124\n");
    
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
        else if (strncmp(input_buffer, "cat ", 4) == 0) {
            const char* filename = input_buffer + 4;
            char file_buf[512];
            if (fat32_read_file(&fs, filename, file_buf, sizeof(file_buf)) == 0) {
                terminal_writestring(file_buf);
                terminal_writestring("\n");
            } else {
                terminal_writestring("Failed to read file: ");
                terminal_writestring(filename);
                terminal_writestring("\n");
            }
        }
        else if (strncmp(input_buffer, "touch ", 6) == 0) {
            const char* filename = input_buffer + 6;
            if (fat32_create_file(&fs, filename) == 0) {
                terminal_writestring("Created file: ");
                terminal_writestring(filename);
                terminal_writestring("\n");
            } else {
                terminal_writestring("Failed to create file.\n");
            }
        }
        else if (strncmp(input_buffer, "mkdir ", 6) == 0) {
            const char* dirname = input_buffer + 6;
            if (fat32_create_dir(&fs, dirname) == 0) {
                terminal_writestring("Created directory: ");
                terminal_writestring(dirname);
                terminal_writestring("\n");
            } else {
                terminal_writestring("Failed to create directory.\n");
            }
        }
        else if (strncmp(input_buffer, "plt", 3) == 0) {
            terminal_writestring("Color Palette:\n");
            for (int i = 1; i <= 15; i++) {
                terminal_putchar_c('#', (uint8_t)i);
            }
            terminal_writestring("\n");
        }
        else if (strncmp(input_buffer, "echo ", 5) == 0) {
            terminal_writestring(input_buffer + 5);
            terminal_writestring("\n");
        }
        else if (strcmp(input_buffer, "help") == 0) {
            terminal_writestring("Commands: logo, ls, clear, ps, cat <file>, touch <file>, mkdir <dir>, echo <text>, plt, help\n");
        } 
        else if (input_buffer[0] != '\0') {
            terminal_writestring("Unknown command: ");
            terminal_writestring(input_buffer);
            terminal_writestring("\n");
        }
    }
}