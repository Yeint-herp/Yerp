#include <arch/CpuCap.h>
#include <arch/Rng.h>
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

    Arch_RngInit();
    const Arch_RngSource rngSource = Arch_RngGetSource();
    if (rngSource == RNG_SRC_NONE)
        Dbg_Log(ERROR, "hal", "no hrng available");
    else
        Dbg_Log(TRACE, "hal", "initialized hrng using %s",
                (rngSource == RNG_SRC_RDSEED)   ? "hardware RdSeed"
                : (rngSource == RNG_SRC_RDRAND) ? "hardware RndRand"
                                                : "hardware TSC");

    Dbg_Log(INFO, "hal", "early initialization done");
}
