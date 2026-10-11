global irq0_handler_stub
extern timer_tick_handler

irq0_handler_stub:
    ; 1. Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Call your C handler (can trigger process_yield)
    call timer_tick_handler

    ; 3. Send End-Of-Interrupt (EOI) to Master PIC
    mov al, 0x20
    out 0x20, al

    ; 4. Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    iretq   ; Return from interrupt (restores CS, RIP, RFLAGS, RSP automatically)