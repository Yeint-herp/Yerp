#include <arch/Rng.h>
#include <debug/DbgPrint.h>
#include <hal/Hal.h>

uptr __stack_chk_guard = 0xDEADCAFEBAADF00D;

[[gnu::no_stack_protector]]
void Core_StackGuardInit(void)
{
    u64 val;

retry:
    if (!Arch_RngFill(&val, sizeof(val)))
    {
        Dbg_Log(ERROR, "StackGuard", "no hardware entropy");
        return;
    }

    /// make the canary contain a null byte in the lowest position.
    /// this helps prevent string-based overflows.
    val &= ~0xFF;

    if (val == 0)
        goto retry;

    __stack_chk_guard = (uptr)val;
}

[[noreturn]]
void __stack_chk_fail(void)
{
    Dbg_Print("*** STACK SMASH DETECTED ***\n");

    Hal_HaltCatchFire();
}
