#include <arch/x86_64/Cpuid.h>

void Arch_CpuidQuery(u32 leaf, u32 subleaf, Arch_CpuidRegs *out)
{
    __asm__ volatile("cpuid"
                     : "=a"(out->Eax), "=b"(out->Ebx), "=c"(out->Ecx), "=d"(out->Edx)
                     : "a"(leaf), "c"(subleaf));
}

u32 Arch_CpuidMaxLeaf(void)
{
    Arch_CpuidRegs r;
    Arch_CpuidQuery(0, 0, &r);

    return r.Eax;
}

u32 Arch_CpuidMaxExtLeaf(void)
{
    Arch_CpuidRegs r;
    Arch_CpuidQuery(0x80000000, 0, &r);

    return r.Eax;
}
