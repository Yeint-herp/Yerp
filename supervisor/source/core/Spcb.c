#define DBG_MODULE "Spcb"

#include <arch/Atomic.h>
#include <arch/CoreLocal.h>
#include <arch/CpuHint.h>
#include <arch/MmArch.h>
#include <boot/Loader.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <dispatcher/Scheduler.h>
#include <executive/Init.h>
#include <executive/Timer.h>
#include <mm/Early.h>
#include <mm/Vas.h>

static struct Core_SPCB *s_SpcbArray = nullptr;
static u32               s_CpuCount  = 0;

static Arch_Atomic32 s_ApReadyCount = {};
static Arch_Atomic32 s_ApRelease    = {};

u32 Core_GetProcessorCount(void)
{
    return s_CpuCount;
}

bool Core_SpcbAllocateAll(void)
{
    const Boot_SmpInfo *smp = Boot_GetSmpInfo();

    if (!smp)
    {
        s_CpuCount = 1;
        Log(TRACE, "falling back to uniprocessor mode");

        usize totalSize = sizeof(struct Core_SPCB);
        s_SpcbArray     = Mm_PermanentAllocate(totalSize, 64);

        if (!s_SpcbArray)
            Panic("Mm_PermanentAllocate failed for UP SPCB!");

        Log(TRACE, "allocated BSP SPCB at %p", s_SpcbArray);
        Core_ZeroMemory(s_SpcbArray, totalSize);

        s_SpcbArray[0].Self            = &s_SpcbArray[0];
        s_SpcbArray[0].ProcessorNumber = 0;

        return false;
    }

    s_CpuCount = smp->Count;
    Log(INFO, "detected %u processors", s_CpuCount);

    usize totalSize = sizeof(struct Core_SPCB) * s_CpuCount;
    s_SpcbArray     = Mm_PermanentAllocate(totalSize, 64);

    if (!s_SpcbArray)
        Panic("Mm_PermanentAllocate failed for %u SMP SPCBs!", s_CpuCount);

    Log(TRACE, "allocated SPCBs array at %p (size %llZ)", s_SpcbArray, totalSize);
    Core_ZeroMemory(s_SpcbArray, totalSize);

    u32 bspIndex = 0;
    for (u32 i = 0; i < s_CpuCount; i++)
        if (smp->ArchIds[i] == smp->BspArchId)
        {
            bspIndex = i;
            break;
        }

    for (u32 i = 0; i < s_CpuCount; i++)
    {
        u32 targetSpcbIndex = i;
        if (i == 0)
            targetSpcbIndex = bspIndex;
        else if (i == bspIndex)
            targetSpcbIndex = 0;

        s_SpcbArray[targetSpcbIndex].Self            = &s_SpcbArray[targetSpcbIndex];
        s_SpcbArray[targetSpcbIndex].ProcessorNumber = targetSpcbIndex;
        s_SpcbArray[targetSpcbIndex].ArchId          = smp->ArchIds[i];

        Boot_SetCpuExtra(i, (uptr)&s_SpcbArray[targetSpcbIndex]);

        Log(TRACE, "SPCB[%u] mapped to ArchId %u at %p", i, s_SpcbArray[targetSpcbIndex].ArchId,
            &s_SpcbArray[targetSpcbIndex]);
    }

    if (!Core_SpcbInit(&s_SpcbArray[0]))
        Panic("Core_SpcbInit failed for BSP SPCB!");

    return true;
}

bool Core_SpcbInit(struct Core_SPCB *spcb)
{
    if (!Arch_SpcrInit(&spcb->ArchData))
    {
        bool isAp = spcb->ProcessorNumber != 0;
        Log(ERROR, "Arch_SpcrInit failed for SPCB[%i]%s", spcb->ProcessorNumber, isAp ? " AP will be offline!" : "");
        return false;
    }

    Arch_SetCoreSpcb(spcb);
    return true;
}

struct Core_SPCB *Core_SpcbGetByNumber(u32 processorNumber)
{
    if (!s_SpcbArray || processorNumber >= s_CpuCount)
        return nullptr;

    return &s_SpcbArray[processorNumber];
}

[[noreturn]] static void s_ApEntry(uptr arg)
{
    struct Core_SPCB *spcb = (struct Core_SPCB *)arg;

    if (!Core_SpcbInit(spcb))
        Ex_HaltCatchFire();

    Arch_MmSwitchRoot(Mm_GetSupervisorVas()->PageTableRoot);
    Arch_MmFlushTlbGlobal();

    Interrupt_InitAp();
    Ex_TimerCpuInit(&spcb->Timers);

    Log(TRACE, "AP[%u] online (ArchId %u)", spcb->ProcessorNumber, spcb->ArchId);
    Arch_AtomicAdd32(&s_ApReadyCount, 1);

    while (!Arch_AtomicLoad32(&s_ApRelease))
        Arch_CpuSleep();

    Ds_SchedulerInitAp();
    Ds_EnterDispatcher();
    unreachable();
}

void Core_SpcbBootAll(void)
{
    if (s_CpuCount <= 1)
    {
        Log(TRACE, "no APs to boot");
        return;
    }

    Arch_AtomicStore32(&s_ApReadyCount, 0);
    Arch_AtomicStore32(&s_ApRelease, 0);

    u32 expectedAps = s_CpuCount - 1;

    Boot_LaunchAps(s_ApEntry);

    while (Arch_AtomicLoad32(&s_ApReadyCount) < expectedAps)
        Arch_CpuRelax();

    Log(INFO, "%u application processors online and parked", expectedAps);
}

void Core_SpcbReleaseAps(void)
{
    if (s_CpuCount <= 1)
        return;

    Log(TRACE, "releasing %u parked APs", s_CpuCount - 1);
    Arch_AtomicStore32(&s_ApRelease, 1);
}
