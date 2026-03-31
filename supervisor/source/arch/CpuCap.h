#ifndef SUPERVISOR_ARCH_CPUCAP_H
#define SUPERVISOR_ARCH_CPUCAP_H

typedef enum : u32
{
    CPUCAP_FPU = 0,
    CPUCAP_SSE,
    CPUCAP_SSE2,
    CPUCAP_SSE3,
    CPUCAP_SSSE3,
    CPUCAP_SSE4_1,
    CPUCAP_SSE4_2,
    CPUCAP_AVX,
    CPUCAP_AVX2,
    CPUCAP_AVX512F,

    CPUCAP_POPCNT,
    CPUCAP_AES,
    CPUCAP_PCLMUL,
    CPUCAP_RDRAND,
    CPUCAP_RDSEED,
    CPUCAP_CX16,
    CPUCAP_MOVBE,
    CPUCAP_BMI1,
    CPUCAP_BMI2,
    CPUCAP_TSC,
    CPUCAP_TSC_DEADLINE,
    CPUCAP_TSC_INVARIANT,
    CPUCAP_X2APIC,

    CPUCAP_PAT,
    CPUCAP_PSE,
    CPUCAP_PGE,
    CPUCAP_NX,
    CPUCAP_1GB_PAGES,
    CPUCAP_PCID,
    CPUCAP_INVPCID,
    CPUCAP_SMEP,
    CPUCAP_SMAP,
    CPUCAP_UMIP,

    CPUCAP_VMX,
    CPUCAP_SVM,

    CPUCAP_IBRS,
    CPUCAP_STIBP,
    CPUCAP_SSBD,

    CPUCAP__COUNT
} Arch_CpuCap;

void Arch_CpuCapInit(void);

bool Arch_CpuHasCap(Arch_CpuCap cap);

typedef enum
{
    kCpuVendor_Unknown = 0,
    kCpuVendor_Intel,
    kCpuVendor_Amd,
} Arch_CpuVendor;

Arch_CpuVendor Arch_CpuGetVendor(void);

const char *Arch_CpuGetBrandString(void);

#endif /* SUPERVISOR_ARCH_CPUCAP_H */
