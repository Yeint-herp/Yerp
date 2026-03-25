#define DBG_MODULE "Hal"

#include <arch/CpuCap.h>
#include <arch/Rng.h>
#include <boot/Limine.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <hal/Hal.h>
#include <mm/Early.h>
#include <mm/MemMap.h>
#include <mm/MmLayout.h>
#include <mm/PfnDb.h>
#include <mm/Pool.h>

void Hal_InitializeEarly()
{
    Dbg_RegisterSinker(g_RingBufSinker);
    Log(TRACE, "early ring buffer sinker ready");

    if (kArch == x86_64)
        Dbg_RegisterSinker(g_e9Sinker);

    Arch_CpuCapInit();
    Log(TRACE, "queried cpu capabilities");

    Arch_RngInit();
    const Arch_RngSource rngSource = Arch_RngGetSource();
    if (rngSource == RNG_SRC_NONE)
        Log(ERROR, "no hrng available");
    else
        Log(TRACE, "initialized hrng using %s",
            (rngSource == RNG_SRC_RDSEED)   ? "hardware RdSeed"
            : (rngSource == RNG_SRC_RDRAND) ? "hardware RndRand"
                                            : "hardware TSC");

    if (!LIMINE_BASE_REVISION_SUPPORTED(Boot_LimineBaseRevision) || !Boot_LimineMemmapReq.response ||
        !Boot_LimineHhdmReq.response)
    {
        Log(FATAL, "critical bootloader requests not fulfilled");

        Hal_HaltCatchFire();
    }
    Mm_EarlyInit(Boot_LimineMemmapReq.response, Boot_LimineHhdmReq.response->offset);

    Mm_DumpMemMap(Mm_GetKernelMemMap());

    bool hasMultiprocessor = Core_SpcbAllocateAll(Boot_LimineSmpReq.response);
    if (!hasMultiprocessor)
        Log(INFO, "multiprocessor capabilities not detected");

    Arch_MmLayoutInit();
    Mm_PfnDbInit();
    Ex_PoolInit();

    Log(INFO, "early initialization done");
}

void Hal_DefaultInterruptHandler(void)
{
    Log(FATAL, "interrupt invoked before handler ready");
    Hal_HaltCatchFire();
}
