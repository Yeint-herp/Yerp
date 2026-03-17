#ifndef SUPERVISOR_ARCH_X86_64_CPUID_INTERNAL_H
#define SUPERVISOR_ARCH_X86_64_CPUID_INTERNAL_H

typedef struct
{
    u32 Eax;
    u32 Ebx;
    u32 Ecx;
    u32 Edx;
} Arch_CpuidRegs;

void Arch_CpuidQuery(u32 leaf, u32 subleaf, Arch_CpuidRegs *out);
u32  Arch_CpuidMaxLeaf(void);
u32  Arch_CpuidMaxExtLeaf(void);

#endif /* SUPERVISOR_ARCH_X86_64_CPUID_INTERNAL_H */
