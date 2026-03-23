#include <arch/Panic.h>
#include <core/Spinlock.h>
#include <core/VarArg.h>
#include <debug/Panic.h>
#include <hal/Hal.h>

#define PANIC_HOOK_MAX 8
#define PANIC_BUF_SIZE 1024

static Panic_Hook s_Hooks[PANIC_HOOK_MAX] = {};
static usize      s_HookCount             = 0;
static bool       s_PanicActive           = false;

bool Panic_RegisterHook(Panic_Hook hook)
{
    if (s_HookCount >= PANIC_HOOK_MAX)
        return false;

    s_Hooks[s_HookCount++] = hook;
    return true;
}

[[noreturn]] static void s_PanicCore(const Arch_RegisterFrame *frame, const char *fmt, Core_VarArgs ap)
{
    if (s_PanicActive)
        Hal_HaltCatchFire();

    s_PanicActive = true;

    Arch_PanicFreeze();

    Dbg_SinkerMask allSinkers = (Dbg_SinkerMask)~0;
    char           buf[PANIC_BUF_SIZE];

    Dbg_FmtContext ctx = {
        .Buffer   = buf,
        .Capacity = sizeof(buf),
        .Position = 0,
        .sinkMask = allSinkers,
    };

    Dbg_FmtString(&ctx, "\n\n*** SUPERVISOR PANIC ***\n", 27);

    Dbg_FmtVprintf(&ctx, fmt, ap);
    Dbg_FmtChar(&ctx, '\n');
    Dbg_FmtFlush(&ctx);

    Dbg_FmtString(&ctx, "\n--- Registers ---\n", 19);
    Arch_PanicDumpRegisters(&ctx, frame);
    Dbg_FmtFlush(&ctx);

    Dbg_FmtString(&ctx, "\n--- Backtrace ---\n", 19);
    Arch_PanicBacktrace(&ctx, frame, 32);
    Dbg_FmtFlush(&ctx);

    for (usize i = 0; i < s_HookCount; i++)
        s_Hooks[i](fmt, frame);

    Dbg_FmtString(&ctx, "\n*** System halted ***\n", 23);
    Dbg_FmtFlush(&ctx);

    Hal_HaltCatchFire();
}

void Panic_WithFrame(const Arch_RegisterFrame *frame, const char *fmt, ...)
{
    Core_VarArgs ap;
    Core_VarArgStart(ap);
    s_PanicCore(frame, fmt, ap);
}

void Panic_(const char *fmt, ...)
{
    Core_VarArgs ap;
    Core_VarArgStart(ap);
    s_PanicCore(nullptr, fmt, ap);
}

void Panic_Fault(const Arch_RegisterFrame *frame, const char *reason)
{
    Panic_WithFrame(frame, "CPU fault: %s (vec = %llu, err = %#llx)", reason, frame->Vector, frame->ErrorCode);
}
