#include <arch/Io.h>
#include <arch/Panic.h>
#include <arch/x86_64/RegisterFrame.h>
#include <debug/DbgPrint.h>

void Arch_PanicFreeze(void)
{
    __asm__ volatile("cli");
}

#define s_Format(val, tmp)                                                                                             \
    char        tmp[16];                                                                                               \
    const char *digits = "0123456789abcdef";                                                                           \
    for (int __i = 15; __i >= 0; __i--)                                                                                \
        tmp[15 - __i] = digits[(val >> (__i * 4)) & 0xF];

static void s_DumpReg(Dbg_FmtContext *ctx, const char *name, u64 val)
{
    Dbg_FmtString(ctx, "  ", 2);

    usize len = 0;
    while (name[len])
        len++;
    Dbg_FmtString(ctx, name, len);
    for (usize i = len; i < 4; i++)
        Dbg_FmtChar(ctx, ' ');

    Dbg_FmtString(ctx, "= ", 2);

    Dbg_FmtChar(ctx, '0');
    Dbg_FmtChar(ctx, 'x');

    s_Format(val, tmp);

    Dbg_FmtString(ctx, tmp, 16);
    Dbg_FmtChar(ctx, '\n');
}

void Arch_PanicDumpRegisters(Dbg_FmtContext *ctx, const Arch_RegisterFrame *frame)
{
    if (!frame)
    {
        Dbg_FmtString(ctx, "(no register frame)\n", 20);

        u64 cr0, cr2, cr3, cr4;
        __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

        s_DumpReg(ctx, "CR0", cr0);
        s_DumpReg(ctx, "CR2", cr2);
        s_DumpReg(ctx, "CR3", cr3);
        s_DumpReg(ctx, "CR4", cr4);
        Dbg_FmtFlush(ctx);

        return;
    }

    s_DumpReg(ctx, "RAX", frame->Rax);
    s_DumpReg(ctx, "RBX", frame->Rbx);
    s_DumpReg(ctx, "RCX", frame->Rcx);
    s_DumpReg(ctx, "RDX", frame->Rdx);
    s_DumpReg(ctx, "RSI", frame->Rsi);
    s_DumpReg(ctx, "RDI", frame->Rdi);
    s_DumpReg(ctx, "RBP", frame->Rbp);
    s_DumpReg(ctx, "RSP", frame->Rsp);
    s_DumpReg(ctx, "R8", frame->R8);
    s_DumpReg(ctx, "R9", frame->R9);
    s_DumpReg(ctx, "R10", frame->R10);
    s_DumpReg(ctx, "R11", frame->R11);
    s_DumpReg(ctx, "R12", frame->R12);
    s_DumpReg(ctx, "R13", frame->R13);
    s_DumpReg(ctx, "R14", frame->R14);
    s_DumpReg(ctx, "R15", frame->R15);
    Dbg_FmtFlush(ctx);

    s_DumpReg(ctx, "RIP", frame->Rip);
    s_DumpReg(ctx, "RFLG", frame->Rflags);
    s_DumpReg(ctx, "CS", frame->Cs);
    s_DumpReg(ctx, "SS", frame->Ss);
    Dbg_FmtFlush(ctx);

    s_DumpReg(ctx, "CR0", frame->Cr0);
    s_DumpReg(ctx, "CR2", frame->Cr2);
    s_DumpReg(ctx, "CR3", frame->Cr3);
    s_DumpReg(ctx, "CR4", frame->Cr4);
    s_DumpReg(ctx, "CR8", frame->Cr8);
    Dbg_FmtFlush(ctx);

    s_DumpReg(ctx, "DR6", frame->Dr6);
    s_DumpReg(ctx, "DR7", frame->Dr7);
    Dbg_FmtFlush(ctx);

    Dbg_FmtFlush(ctx);
}

void Arch_PanicBacktrace(Dbg_FmtContext *ctx, const Arch_RegisterFrame *frame, usize maxFrames)
{
    if (!frame)
    {
        Dbg_FmtString(ctx, "(no frame pointer available)\n", 29);
        return;
    }

    struct StackFrame
    {
        struct StackFrame *prev;
        u64                returnAddr;
    };

    struct StackFrame *fp = (struct StackFrame *)frame->Rbp;

    for (usize i = 0; i < maxFrames && fp; i++)
    {
        uptr addr = (uptr)fp;
        if (addr == 0 || (addr & 0x7) != 0)
            break;

        if ((addr >> 47) != 0 && (addr >> 47) != 0x1FFFF)
            break;

        Dbg_FmtString(ctx, "  #", 3);

        if (i < 10)
            Dbg_FmtChar(ctx, '0' + (char)i);
        else
        {
            Dbg_FmtChar(ctx, '0' + (char)(i / 10));
            Dbg_FmtChar(ctx, '0' + (char)(i % 10));
        }

        Dbg_FmtString(ctx, "  ", 2);

        Dbg_FmtChar(ctx, '0');
        Dbg_FmtChar(ctx, 'x');

        u64 val = fp->returnAddr;
        s_Format(val, tmp);

        Dbg_FmtString(ctx, tmp, 16);
        Dbg_FmtChar(ctx, '\n');

        fp = fp->prev;
    }

    Dbg_FmtFlush(ctx);
}
