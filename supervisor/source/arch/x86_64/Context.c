#include <arch/Context.h>

void Arch_ContextInit(Arch_ThreadContext *ctx, uptr stackTop, Arch_ThreadEntry entry, void *parameter)
{
    uptr *sp = (uptr *)(stackTop & ~0xFULL);

    *--sp = (uptr)Arch_ThreadTrampoline;
    *--sp = 0x202;
    *--sp = 0;
    *--sp = 0;
    *--sp = (uptr)parameter;
    *--sp = (uptr)entry;
    *--sp = 0;
    *--sp = 0;

    ctx->Sp = (uptr)sp;
}
