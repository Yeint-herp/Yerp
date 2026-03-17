#include <debug/DbgPrint.h>
#include <hal/Hal.h>

uptr __stack_chk_guard = 0xDEADCAFEBAADF00D;

void Core_StackGuardInit(void)
{
    /* TODO */
}

[[noreturn]]
void __stack_chk_fail(void)
{
    Dbg_Print("*** STACK SMASH DETECTED ***\n");

    Hal_HaltCatchFire();
}
