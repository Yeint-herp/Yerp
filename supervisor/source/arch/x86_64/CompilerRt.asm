.intel_syntax noprefix

.section .text, "ax", @progbits
.p2align 4, 0x90

.globl __udivmodti4
.type __udivmodti4, @function
__udivmodti4:
    .cfi_startproc

    push rbx
    .cfi_def_cfa_offset 16
    .cfi_offset rbx, -16
    push r12
    .cfi_def_cfa_offset 24
    .cfi_offset r12, -24
    push r13
    .cfi_def_cfa_offset 32
    .cfi_offset r13, -32
    push r14
    .cfi_def_cfa_offset 40
    .cfi_offset r14, -40
    push r15
    .cfi_def_cfa_offset 48
    .cfi_offset r15, -48

    mov r9, rdi
    mov r10, rsi
    mov r11, rdx
    mov r12, rcx
    mov r13, r8

    mov rax, r11
    or rax, r12
    jz .div_by_zero

    test r12, r12
    jnz .full_128

    test r10, r10
    jnz .den64_num128

    xor edx, edx
    mov rax, r9
    div r11

    test r13, r13
    jz .done_fast64

    mov qword ptr [r13], rdx
    mov qword ptr [r13 + 8], 0
.done_fast64:
    xor edx, edx
    jmp .epilogue

.den64_num128:
    xor edx, edx
    mov rax, r10
    div r11
    mov r14, rax

    mov rax, r9
    div r11

    test r13, r13
    jz .done_den64
    mov qword ptr [r13], rdx
    mov qword ptr [r13 + 8], 0
.done_den64:
    mov rdx, r14
    jmp .epilogue

.full_128:
    bsr rcx, r12
    add rcx, 64

    bsr rbx, r10
    jz .num_hi_zero_full

    add rbx, 64
    jmp .got_num_msb

.num_hi_zero_full:
    bsr rbx, r9
    jz .quotient_zero
.got_num_msb:
    cmp rbx, rcx
    jb .quotient_zero

    sub rbx, rcx
    mov rcx, rbx

    mov r14, r11
    mov r15, r12

    cmp cl, 64
    jae .shift_ge64

    shld r15, r14, cl
    shl r14, cl
    jmp .shift_done

.shift_ge64:
    mov r15, r14
    xor r14d, r14d
    sub cl, 64
    shl r15, cl
    add cl, 64

.shift_done:
    xor ebx, ebx
    xor edi, edi

    inc rcx
.div_loop:
    shld rdi, rbx, 1
    shl rbx, 1

    cmp r10, r15
    ja .subtract
    jb .no_subtract

    cmp r9, r14
    jb .no_subtract
.subtract:
    sub r9, r14
    sbb r10, r15

    or rbx, 1
.no_subtract:
    shrd r14, r15, 1
    shr r15, 1

    dec rcx
    jnz .div_loop

    test r13, r13
    jz .done_full

    mov qword ptr [r13], r9
    mov qword ptr [r13 + 8], r10
.done_full:
    mov rax, rbx
    mov rdx, rdi
    jmp .epilogue

.quotient_zero:
    test r13, r13
    jz .ret_zero

    mov qword ptr [r13], r9
    mov qword ptr [r13 + 8], r10
.ret_zero:
    xor eax, eax
    xor edx, edx
    jmp .epilogue

.div_by_zero:
    xor eax, eax
    div eax

.epilogue:
    pop r15
    .cfi_def_cfa_offset 40
    pop r14
    .cfi_def_cfa_offset 32
    pop r13
    .cfi_def_cfa_offset 24
    pop r12
    .cfi_def_cfa_offset 16
    pop rbx
    .cfi_def_cfa_offset 8
    ret

    .cfi_endproc
.size __udivmodti4, . - __udivmodti4

.globl __udivti3
.type __udivti3, @function
__udivti3:
    .cfi_startproc

    xor r8d, r8d
    jmp __udivmodti4

    .cfi_endproc
.size __udivti3, . - __udivti3

.globl __umodti3
.type __umodti3, @function
__umodti3:
    .cfi_startproc

    push rbp
    .cfi_def_cfa_offset 16
    .cfi_offset rbp, -16
    mov rbp, rsp
    .cfi_def_cfa_register rbp

    sub rsp, 16

    lea r8, qword ptr [rsp]
    call __udivmodti4

    mov rax, qword ptr  [rsp]
    mov rdx, qword ptr [rsp + 8]

    add rsp, 16
    pop rbp
    .cfi_def_cfa rsp, 8
    ret

    .cfi_endproc
.size __umodti3, . - __umodti3
