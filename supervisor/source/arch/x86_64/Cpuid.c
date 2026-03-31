#include <arch/x86_64/Cpuid.h>

void X86_64_CpuidQuery(u32 leaf, u32 subleaf, Arch_CpuidRegs *out)
{
    __asm__ volatile("cpuid"
                     : "=a"(out->Eax), "=b"(out->Ebx), "=c"(out->Ecx), "=d"(out->Edx)
                     : "a"(leaf), "c"(subleaf));
}

u32 X86_64_CpuidMaxLeaf(void)
{
    Arch_CpuidRegs r;
    X86_64_CpuidQuery(0, 0, &r);

    return r.Eax;
}

u32 X86_64_CpuidMaxExtLeaf(void)
{
    Arch_CpuidRegs r;
    X86_64_CpuidQuery(0x80000000, 0, &r);

    return r.Eax;
}
