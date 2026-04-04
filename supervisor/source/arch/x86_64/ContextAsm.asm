.intel_syntax noprefix

.section .text, "ax", @progbits
.p2align 4, 0x90

.global Arch_ContextSwitch
.type Arch_ContextSwitch, @function
Arch_ContextSwitch:
    .cfi_startproc

    pushfq
    .cfi_def_cfa_offset 16

    push rbp
    .cfi_def_cfa_offset 24
    .cfi_offset rbp, -24

    push rbx
    .cfi_def_cfa_offset 32
    .cfi_offset rbx, -32

    push r12
    .cfi_def_cfa_offset 40
    .cfi_offset r12, -40

    push r13
    .cfi_def_cfa_offset 48
    .cfi_offset r13, -48

    push r14
    .cfi_def_cfa_offset 56
    .cfi_offset r14, -56

    push r15
    .cfi_def_cfa_offset 64
    .cfi_offset r15, -64

    mov [rdi], rsp
    mov rsp, rsi 

    pop r15
    .cfi_def_cfa_offset 56
    .cfi_restore r15

    pop r14
    .cfi_def_cfa_offset 48
    .cfi_restore r14

    pop r13
    .cfi_def_cfa_offset 40
    .cfi_restore r13

    pop r12
    .cfi_def_cfa_offset 32
    .cfi_restore r12

    pop rbx
    .cfi_def_cfa_offset 24
    .cfi_restore rbx

    pop rbp
    .cfi_def_cfa_offset 16
    .cfi_restore rbp

    popfq
    .cfi_def_cfa_offset 8

    ret
    .cfi_endproc
.size Arch_ContextSwitch, . - Arch_ContextSwitch

.global Arch_ThreadTrampoline
.type Arch_ThreadTrampoline, @function
Arch_ThreadTrampoline:
    .cfi_startproc

    .cfi_undefined rip

    sti
    mov rdi, r12
    call r13
    call Ds_ThreadExit
    ud2

    .cfi_endproc
.size Arch_ThreadTrampoline, . - Arch_ThreadTrampoline
