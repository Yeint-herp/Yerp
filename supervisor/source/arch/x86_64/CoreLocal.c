#include <arch/CoreLocal.h>
#include <arch/x86_64/Msr.h>

void Arch_SetCoreSpcb(struct Core_SPCB *spcb)
{
    X86_64_WriteMsr(X86_64_MSR_GS_BASE, (uptr)spcb);
}

struct Core_SPCB *Arch_GetCurrentSpcb(void)
{
    struct Core_SPCB *spcb;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(spcb));

    return spcb;
}
