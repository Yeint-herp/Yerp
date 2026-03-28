#define DBG_MODULE "Spcb"

#include <arch/CoreLocal.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Init.h>
#include <limine.h>
#include <mm/Early.h>

static struct Core_SPCB *s_SpcbArray = nullptr;
static u32               s_CpuCount  = 0;

u32 Core_GetProcessorCount(void)
{
    return s_CpuCount;
}

bool Core_SpcbAllocateAll(struct limine_mp_response *mpResponse)
{
    if (!mpResponse || mpResponse->cpu_count <= 1)
    {
        s_CpuCount = 1;
        Log(TRACE, "falling back to uniprocessor mode.");

        usize totalSize = sizeof(struct Core_SPCB);
        s_SpcbArray     = Mm_PermanentAllocate(totalSize, 64);

        if (!s_SpcbArray)
            Panic("Mm_EarlyAllocate failed for UP SPCB!");

        Log(TRACE, "allocated BSP SPCB at %p", s_SpcbArray);
        Core_ZeroMemory(s_SpcbArray, totalSize);

        s_SpcbArray[0].Self            = &s_SpcbArray[0];
        s_SpcbArray[0].ProcessorNumber = 0;

        return false;
    }

    s_CpuCount = mpResponse->cpu_count;
    Log(INFO, "detected %u processors.", s_CpuCount);

    usize totalSize = sizeof(struct Core_SPCB) * s_CpuCount;
    s_SpcbArray     = Mm_PermanentAllocate(totalSize, 64);

    if (!s_SpcbArray)
        Panic("Mm_EarlyAllocate failed for %u SMP SPCBs!", s_CpuCount);

    Log(TRACE, "allocated SPCBs array at %p (size %llZ)", s_SpcbArray, totalSize);
    Core_ZeroMemory(s_SpcbArray, totalSize);

    const ArchId_t archIdBsp = Arch_GetBspArchId(mpResponse);
    u32            bspIndex  = 0;
    for (u32 i = 0; i < s_CpuCount; i++)
        if (Arch_GetArchId(mpResponse->cpus[i]) == archIdBsp)
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
        s_SpcbArray[targetSpcbIndex].ArchId          = Arch_GetArchId(mpResponse->cpus[i]);

        mpResponse->cpus[i]->extra_argument = (uptr)&s_SpcbArray[targetSpcbIndex];

        Log(TRACE, "SPCB[%u] mapped to ArchId %u at %p", i, s_SpcbArray[targetSpcbIndex].ArchId,
            &s_SpcbArray[targetSpcbIndex]);
    }

    if (!Core_SpcbInit(&s_SpcbArray[0]))
        Panic("Core_SpcbInit failed for BSP SPCR!");

    return true;
}

bool Core_SpcbInit(struct Core_SPCB *spcb)
{
    if (!Arch_SpcrInit(&spcb->ArchData))
    {
        bool isAp = spcb->ProcessorNumber != 0;
        Log(ERROR, "Arch_SpcrInit failed for SPCR[%i]%s", spcb->ProcessorNumber, isAp ? " AP will be offline!" : "");

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
