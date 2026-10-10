global switch_to

section .text
bits 64

; void switch_to(uint64_t *old_rsp, uint64_t new_rsp);
switch_to:
    ; Save callee-saved registers onto the current stack
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save the current stack pointer into *old_rsp
    mov [rdi], rsp

    ; Load the new stack pointer from new_rsp
    mov rsp, rsi

    ; Restore callee-saved registers from the new stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret