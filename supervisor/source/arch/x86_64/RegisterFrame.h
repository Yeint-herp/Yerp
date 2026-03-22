#ifndef SUPERVISOR_ARCH_X86_64_REGISTER_FRAME_H
#define SUPERVISOR_ARCH_X86_64_REGISTER_FRAME_H

struct [[gnu::packed]] X86_64_RegisterFrame
{
    u64 Cr2;
    u64 Cr3;
    u64 Cr4;
    u64 Cr0;
    u64 Cr8;
    u64 Dr6;
    u64 Dr7;

    u64 R15;
    u64 R14;
    u64 R13;
    u64 R12;
    u64 R11;
    u64 R10;
    u64 R9;
    u64 R8;
    u64 Rax;
    u64 Rbx;
    u64 Rcx;
    u64 Rdx;
    u64 Rsi;
    u64 Rdi;
    u64 Rbp;

    u64 RspBase;

    u64 Vector;
    u64 ErrorCode;

    u64 Rip;
    u64 Cs;
    u64 Rflags;
    u64 Rsp;
    u64 Ss;
};

typedef struct X86_64_RegisterFrame Arch_RegisterFrame;

#endif /* SUPERVISOR_ARCH_X86_64_REGISTER_FRAME_H */
