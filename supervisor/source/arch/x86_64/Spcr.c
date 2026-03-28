#define DBG_MODULE "Spcr"

#include <arch/x86_64/Spcr.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <executive/Init.h>
#include <limine.h>

bool Arch_SpcrInit(struct Arch_SPCR *spcr)
{
    X86_64_TssInit(&spcr->Tss);
    X86_64_GdtInit(&spcr->Gdt, &spcr->Tss);
    X86_64_IdtInit(&spcr->Idt);

    for (int i = 0; i < 256; i++)
        spcr->IsrTable[i].Routine = (uptr)Exec_DefaultInterruptHandler;

    Log(TRACE, "SPCR[%i] early initialized", container_of(spcr, struct Core_SPCB, ArchData)->ProcessorNumber);

    return true;
}

ArchId_t Arch_GetBspArchId(const struct limine_mp_response *mpResponse)
{
    if (!mpResponse)
        return 0;

    return mpResponse->bsp_lapic_id;
}

ArchId_t Arch_GetArchId(const struct limine_mp_info *mpInfo)
{
    if (!mpInfo)
        return 0;

    return mpInfo->lapic_id;
}
