global start
extern kernel_main
extern __bss_start
extern __bss_end

section .text
bits 32
start:
    mov esp, stack_top

    cmp eax, 0x36d76289
    jne .no_multiboot

    ; Zero out BSS section safely
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor al, al
    rep stosb

    call setup_page_tables
    call enable_paging

    lgdt [gdt64.pointer]
    jmp gdt64.code_selector:kernel_64

.no_multiboot:
    cli
    hlt

setup_page_tables:
    mov eax, pdpt_table
    or eax, 0b11
    mov [pml4_table], eax

    mov eax, pd_table
    or eax, 0b11
    mov [pdpt_table], eax

    mov ecx, 0
.loop:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [pd_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .loop
    ret

enable_paging:
    mov eax, pml4_table
    mov cr3, eax

    ; Enable PAE and SSE (OSFXSR, OSXMMEXCPT) in CR4
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    ; Enable Long Mode in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable Paging in CR0
    mov eax, cr0
    or eax, 1 << 31 | 1 << 0
    mov cr0, eax
    ret

bits 64
kernel_64:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 4096
pml4_table: resb 4096
pdpt_table: resb 4096
pd_table:   resb 4096
stack_bottom: resb 16384
stack_top:

section .rodata
gdt64:
    dq 0 ; Null descriptor
.code_selector equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) | (1 << 55)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .multiboot
align 8
multiboot_header:
    dd 0xe85250d6
    dd 0
    dd header_end - multiboot_header
    dd -(0xe85250d6 + 0 + (header_end - multiboot_header))

    dw 0
    dw 0
    dd 8
header_end: