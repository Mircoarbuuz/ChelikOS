global start
extern kernel_main
extern __bss_start
extern __bss_end

section .data
saved_magic dd 0
saved_info  dd 0

section .text
bits 32
start:
    mov esp, stack_top

    cmp eax, 0x36d76289
    jne .no_multiboot

    ; Save multiboot magic and info pointer
    mov [saved_magic], eax
    mov [saved_info], ebx

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
    ; Link PML4 to PDPT
    mov eax, pdpt_table
    or eax, 0b11
    mov [pml4_table], eax

    ; Point first 4 entries of PDPT to our 4 PD tables (covers 4 GB total)
    xor ecx, ecx
.setup_pdpt:
    mov eax, pd_tables
    mov edx, ecx
    shl edx, 12           ; edx = ecx * 4096
    add eax, edx
    or eax, 0b11
    mov [pdpt_table + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .setup_pdpt

    ; Map all 2048 entries (4 GB total) using 2MB huge pages
    xor ecx, ecx
.loop:
    mov eax, 0x200000     ; 2 MB per page
    mul ecx               ; EAX = ecx * 2 MB
    or eax, 0b10000011    ; Present, Writable, Huge page (2MB)
    mov [pd_tables + ecx * 8], eax
    inc ecx
    cmp ecx, 2048         ; 4 tables * 512 entries = 2048 entries (4GB)
    jne .loop
    ret

enable_paging:
    mov eax, pml4_table
    mov cr3, eax

    ; Enable PAE and SSE in CR4
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

    ; Load saved multiboot arguments into rdi and rsi (System V AMD64 ABI)
    mov edi, dword [rel saved_magic]
    mov esi, dword [rel saved_info]

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 4096
pml4_table: resb 4096
pdpt_table: resb 4096
pd_tables:  resb 4096 * 4     ; 4 Page Directory tables covering 4 GB
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

    ; Framebuffer Request Tag for Mode 0x117 (1024x768x16)
    align 8
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 16

    ; End Tag
    align 8
    dw 0
    dw 0
    dd 8
header_end: