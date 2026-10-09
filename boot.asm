BITS 32

section .multiboot
align 4
header_start:
    dd 0xE85250D6                ; Multiboot 2 magic number
    dd 0                         ; Architecture 0 (Protected mode / i386)
    dd header_end - header_start ; Header length
    dd -(0xE85250D6 + 0 + (header_end - header_start)) ; Checksum

    ; Required End Tag
    dw 0                         ; Type = 0
    dw 0                         ; Flags = 0
    dd 8                         ; Size = 8
header_end:

section .text
global loader
extern kernel_main

loader:
    cli                    ; Disable interrupts
    mov esp, stack_top

    ; 1. Zero out 12KB (3 pages: PML4, PDPT, PD) starting at 0x10000
    mov edi, 0x10000
    xor eax, eax
    mov ecx, (4096 * 3) / 4
    rep stosd

    ; 2. Set up 64-bit Identity Paging at 0x10000
    mov dword [0x10000], 0x11003  ; PML4[0] -> PDPT (0x11000) [Present, Writeable]
    mov dword [0x11000], 0x12003  ; PDPT[0] -> PD (0x12000)   [Present, Writeable]
    mov dword [0x12000], 0x00000083 ; PD[0] -> 2MB Page       [Present, Writeable, Huge]

    ; 3. Load CR3 with PML4 base address
    mov eax, 0x10000
    mov cr3, eax

    ; 4. Enable PAE (Physical Address Extension) in CR4
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 5. Enable Long Mode in EFER MSR (0xC0000080)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 6. Enable Paging and Protected Mode in CR0
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    ; 7. Load 64-bit GDT and jump to 64-bit code segment
    lgdt [gdt64_descriptor]
    jmp 0x08:code_64

section .data
align 8
gdt64:
    dw 0, 0, 0, 0                ; Null descriptor
    dw 0xFFFF, 0x0000, 0x9A00, 0x00AF ; 64-bit code descriptor
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF ; 64-bit data descriptor

gdt64_descriptor:
    dw $ - gdt64 - 1
    dd gdt64

[bits 64]
code_64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, 0x90000     ; Safe 64-bit stack
    call kernel_main     ; Jump into your C code

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384           ; 16 KB Stack
stack_top: