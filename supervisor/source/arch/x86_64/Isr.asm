.intel_syntax noprefix

#include <generated_offsets.inc>

.equ TF_EXTRA_OFF, 0
.equ TF_CR2_OFF, TF_EXTRA_OFF + 0 * 8
.equ TF_CR3_OFF, TF_EXTRA_OFF + 1 * 8
.equ TF_CR4_OFF, TF_EXTRA_OFF + 2 * 8
.equ TF_CR0_OFF, TF_EXTRA_OFF + 3 * 8
.equ TF_CR8_OFF, TF_EXTRA_OFF + 4 * 8
.equ TF_DR6_OFF, TF_EXTRA_OFF + 5 * 8
.equ TF_DR7_OFF, TF_EXTRA_OFF + 6 * 8

.equ TF_REGS_OFF, 56
.equ TF_R15_OFF, TF_REGS_OFF + 0 * 8
.equ TF_R14_OFF, TF_REGS_OFF + 1 * 8
.equ TF_R13_OFF, TF_REGS_OFF + 2 * 8
.equ TF_R12_OFF, TF_REGS_OFF + 3 * 8
.equ TF_R11_OFF, TF_REGS_OFF + 4 * 8
.equ TF_R10_OFF, TF_REGS_OFF + 5 * 8
.equ TF_R9_OFF, TF_REGS_OFF + 6 * 8
.equ TF_R8_OFF, TF_REGS_OFF + 7 * 8
.equ TF_RAX_OFF, TF_REGS_OFF + 8 * 8
.equ TF_RBX_OFF, TF_REGS_OFF + 9 * 8
.equ TF_RCX_OFF, TF_REGS_OFF + 10 * 8
.equ TF_RDX_OFF, TF_REGS_OFF + 11 * 8
.equ TF_RSI_OFF, TF_REGS_OFF + 12 * 8
.equ TF_RDI_OFF, TF_REGS_OFF + 13 * 8
.equ TF_RBP_OFF, TF_REGS_OFF + 14 * 8
.equ TF_RSP_OFF, TF_REGS_OFF + 15 * 8

.equ TF_VEC_OFF, 56 + 128
.equ TF_ERR_OFF, TF_VEC_OFF + 8

.equ TF_HW_OFF, TF_ERR_OFF + 8
.equ TF_RIP_OFF, TF_HW_OFF + 0 * 8
.equ TF_CS_OFF,  TF_HW_OFF + 1 * 8
.equ TF_RFLAGS_OFF, TF_HW_OFF + 2 * 8
.equ TF_HW_RSP_OFF, TF_HW_OFF + 3 * 8
.equ TF_SS_OFF,  TF_HW_OFF + 4 * 8

.equ TF_SIZE, 240

.section .text, "ax", @progbits
.p2align 4, 0x90

.globl IsrCommonInterruptStub
.type IsrCommonInterruptStub, @function
IsrCommonInterruptStub:
    .cfi_startproc simple
    .cfi_signal_frame

    .cfi_def_cfa rsp, 56
    test qword ptr [rsp + 24], 3
    jz .ring0

    swapgs
.ring0:
    sub rsp, TF_REGS_OFF + (16 * 8)
    .cfi_adjust_cfa_offset TF_REGS_OFF + (16 * 8)

    mov qword ptr [rsp + TF_R15_OFF], r15
    .cfi_offset r15, - (TF_SIZE - TF_R15_OFF)
    mov qword ptr [rsp + TF_R14_OFF], r14
    .cfi_offset r14, - (TF_SIZE - TF_R14_OFF)
    mov qword ptr [rsp + TF_R13_OFF], r13
    .cfi_offset r13, - (TF_SIZE - TF_R13_OFF)
    mov qword ptr [rsp + TF_R12_OFF], r12
    .cfi_offset r12, - (TF_SIZE - TF_R12_OFF)
    mov qword ptr [rsp + TF_R11_OFF], r11
    .cfi_offset r11, - (TF_SIZE - TF_R11_OFF)
    mov qword ptr [rsp + TF_R10_OFF], r10
    .cfi_offset r10, - (TF_SIZE - TF_R10_OFF)
    mov qword ptr [rsp + TF_R9_OFF], r9
    .cfi_offset r9, - (TF_SIZE - TF_R9_OFF)
    mov qword ptr [rsp + TF_R8_OFF], r8
    .cfi_offset r8, - (TF_SIZE - TF_R8_OFF)
    mov qword ptr [rsp + TF_RAX_OFF], rax
    .cfi_offset rax, - (TF_SIZE - TF_RAX_OFF)
    mov qword ptr [rsp + TF_RBX_OFF], rbx
    .cfi_offset rbx, - (TF_SIZE - TF_RBX_OFF)
    mov qword ptr [rsp + TF_RCX_OFF], rcx
    .cfi_offset rcx, - (TF_SIZE - TF_RCX_OFF)
    mov qword ptr [rsp + TF_RDX_OFF], rdx
    .cfi_offset rdx, - (TF_SIZE - TF_RDX_OFF)
    mov qword ptr [rsp + TF_RSI_OFF], rsi
    .cfi_offset rsi, - (TF_SIZE - TF_RSI_OFF)
    mov qword ptr [rsp + TF_RDI_OFF], rdi
    .cfi_offset rdi, - (TF_SIZE - TF_RDI_OFF)
    mov qword ptr [rsp + TF_RBP_OFF], rbp
    .cfi_offset rbp, - (TF_SIZE - TF_RBP_OFF)

    lea rax, [rsp + TF_VEC_OFF]
    mov qword ptr [rsp + TF_RSP_OFF], rax

    mov rax, cr2
    mov qword ptr [rsp + TF_CR2_OFF], rax
    mov rax, cr3
    mov qword ptr [rsp + TF_CR3_OFF], rax
    mov rax, cr4
    mov qword ptr [rsp + TF_CR4_OFF], rax
    mov rax, cr0
    mov qword ptr [rsp + TF_CR0_OFF], rax
    mov rax, cr8
    mov qword ptr [rsp + TF_CR8_OFF], rax
    mov rax, dr6
    mov qword ptr [rsp + TF_DR6_OFF], rax
    mov rax, dr7
    mov qword ptr [rsp + TF_DR7_OFF], rax

    mov rdi, rsp

    mov rbx, qword ptr [rsp + TF_VEC_OFF]
    shl rbx, 4

    mov rsi, qword ptr gs:[X86_64_SPCB_ISR_TABLE_OFFSET + rbx + 8]
    mov rax, qword ptr gs:[X86_64_SPCB_ISR_TABLE_OFFSET + rbx]

    cld
    call rax

    mov r15, qword ptr [rsp + TF_R15_OFF]
    mov r14, qword ptr [rsp + TF_R14_OFF]
    mov r13, qword ptr [rsp + TF_R13_OFF]
    mov r12, qword ptr [rsp + TF_R12_OFF]
    mov r11, qword ptr [rsp + TF_R11_OFF]
    mov r10, qword ptr [rsp + TF_R10_OFF]
    mov r9,  qword ptr [rsp + TF_R9_OFF]
    mov r8,  qword ptr [rsp + TF_R8_OFF]
    mov rax, qword ptr [rsp + TF_RAX_OFF]
    mov rbx, qword ptr [rsp + TF_RBX_OFF]
    mov rcx, qword ptr [rsp + TF_RCX_OFF]
    mov rdx, qword ptr [rsp + TF_RDX_OFF]
    mov rsi, qword ptr [rsp + TF_RSI_OFF]
    mov rdi, qword ptr [rsp + TF_RDI_OFF]
    mov rbp, qword ptr [rsp + TF_RBP_OFF]

    add rsp, TF_HW_OFF

    test qword ptr [rsp + 8], 3
    jz .return_ring0

    swapgs
.return_ring0:
    iretq

    .cfi_endproc
.size IsrCommonInterruptStub, .-IsrCommonInterruptStub

.section .text
.set V, -1

.macro make_isr_stub
    .set V, V+1
    .p2align 4, 0x90
    .globl IsrStub_\@
    .type IsrStub_\@, @function
IsrStub_\@:
    .cfi_startproc simple
    .cfi_signal_frame
    .cfi_def_cfa rsp, 40

    .if !((V)==8 || (V)==10 || (V)==11 || (V)==12 || \
          (V)==13 || (V)==14 || (V)==17 || (V)==21 || (V)==29 || (V)==30)
        push 0
        .cfi_adjust_cfa_offset 8
    .endif

    push V
    .cfi_adjust_cfa_offset 8
    jmp IsrCommonInterruptStub

    .cfi_endproc
    .size IsrStub_\@, .-IsrStub_\@

    .pushsection .data.isr_table, "aw"
    .quad IsrStub_\@
    .popsection
.endm

.section .data.isr_table, "aw"
.p2align 3
.globl X86_64_IsrStubTable
X86_64_IsrStubTable:

.section .text
.rept 256
    make_isr_stub
.endr
