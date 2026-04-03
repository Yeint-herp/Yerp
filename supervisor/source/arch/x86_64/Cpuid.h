#ifndef SUPERVISOR_ARCH_X86_64_CPUID_INTERNAL_H
#define SUPERVISOR_ARCH_X86_64_CPUID_INTERNAL_H

typedef struct
{
    u32 Eax;
    u32 Ebx;
    u32 Ecx;
    u32 Edx;
} X86_64_CpuidRegs;

void X86_64_CpuidQuery(u32 leaf, u32 subleaf, X86_64_CpuidRegs *out);
u32  X86_64_CpuidMaxLeaf(void);
u32  X86_64_CpuidMaxExtLeaf(void);

#endif /* SUPERVISOR_ARCH_X86_64_CPUID_INTERNAL_H */
