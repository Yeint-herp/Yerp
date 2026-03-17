#include <arch/CpuCap.h>
#include <debug/DbgPrint.h>
#include <hal/Hal.h>

void Hal_InitializeEarly()
{
    Dbg_RegisterSinker(g_RingBufSinker);
    Dbg_Log(TRACE, "hal", "early ring buffer sinker ready");

    if (kArch == x86_64)
        Dbg_RegisterSinker(g_e9Sinker);

    Arch_CpuCapInit();
    Dbg_Log(TRACE, "hal", "queried cpu capabilities");

    Dbg_Log(INFO, "hal", "early initialization done");
}
