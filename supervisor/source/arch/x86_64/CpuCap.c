#include <arch/CpuCap.h>
#include <arch/x86_64/Cpuid.h>

#define CAP_WORDS ((CPUCAP__COUNT + 31) / 32)

static u32            s_CapBitmap[CAP_WORDS];
static Arch_CpuVendor s_Vendor;
static char           s_Brand[49];

static void s_SetCap(Arch_CpuCap cap)
{
    s_CapBitmap[cap / 32] |= 1UL << (cap % 32);
}

static void s_MapBit(u32 reg, u32 bit, Arch_CpuCap cap)
{
    if (reg & (1UL << bit))
        s_SetCap(cap);
}

static void s_DetectVendor(const Arch_CpuidRegs *leaf0)
{
    if (leaf0->Ebx == 0x756E6547 && leaf0->Edx == 0x49656E69 && leaf0->Ecx == 0x6C65746E)
        s_Vendor = kCpuVendor_Intel;
    else if (leaf0->Ebx == 0x68747541 && leaf0->Edx == 0x69746E65 && leaf0->Ecx == 0x444D4163)
        s_Vendor = kCpuVendor_Amd;
    else
        s_Vendor = kCpuVendor_Unknown;
}

static void s_DetectBrand(u32 maxExt)
{
    if (maxExt < 0x80000004)
    {
        s_Brand[0] = '\0';
        return;
    }

    u32 *dst = (u32 *)s_Brand;
    for (u32 leaf = 0x80000002; leaf <= 0x80000004; leaf++)
    {
        Arch_CpuidRegs r;
        X86_64_CpuidQuery(leaf, 0, &r);
        *dst++ = r.Eax;
        *dst++ = r.Ebx;
        *dst++ = r.Ecx;
        *dst++ = r.Edx;
    }

    s_Brand[48] = '\0';
}

static void s_DetectStdFeatures(u32 maxLeaf)
{
    if (maxLeaf < 1)
        return;

    Arch_CpuidRegs r;
    X86_64_CpuidQuery(1, 0, &r);

    s_MapBit(r.Ecx, 0, CPUCAP_SSE3);
    s_MapBit(r.Ecx, 1, CPUCAP_PCLMUL);
    s_MapBit(r.Ecx, 9, CPUCAP_SSSE3);
    s_MapBit(r.Ecx, 13, CPUCAP_CX16);
    s_MapBit(r.Ecx, 19, CPUCAP_SSE4_1);
    s_MapBit(r.Ecx, 20, CPUCAP_SSE4_2);
    s_MapBit(r.Ecx, 21, CPUCAP_X2APIC);
    s_MapBit(r.Ecx, 22, CPUCAP_MOVBE);
    s_MapBit(r.Ecx, 23, CPUCAP_POPCNT);
    s_MapBit(r.Ecx, 24, CPUCAP_TSC_DEADLINE);
    s_MapBit(r.Ecx, 25, CPUCAP_AES);
    s_MapBit(r.Ecx, 28, CPUCAP_AVX);
    s_MapBit(r.Ecx, 30, CPUCAP_RDRAND);
    s_MapBit(r.Ecx, 5, CPUCAP_VMX);

    s_MapBit(r.Edx, 0, CPUCAP_FPU);
    s_MapBit(r.Edx, 3, CPUCAP_PSE);
    s_MapBit(r.Edx, 4, CPUCAP_TSC);
    s_MapBit(r.Edx, 13, CPUCAP_PGE);
    s_MapBit(r.Edx, 16, CPUCAP_PAT);
    s_MapBit(r.Edx, 25, CPUCAP_SSE);
    s_MapBit(r.Edx, 26, CPUCAP_SSE2);
}

static void s_DetectExtFeatures(u32 maxLeaf)
{
    if (maxLeaf < 7)
        return;

    Arch_CpuidRegs r;
    X86_64_CpuidQuery(7, 0, &r);

    s_MapBit(r.Ebx, 3, CPUCAP_BMI1);
    s_MapBit(r.Ebx, 5, CPUCAP_AVX2);
    s_MapBit(r.Ebx, 7, CPUCAP_SMEP);
    s_MapBit(r.Ebx, 8, CPUCAP_BMI2);
    s_MapBit(r.Ebx, 10, CPUCAP_INVPCID);
    s_MapBit(r.Ebx, 16, CPUCAP_AVX512F);
    s_MapBit(r.Ebx, 18, CPUCAP_RDSEED);
    s_MapBit(r.Ebx, 20, CPUCAP_SMAP);

    s_MapBit(r.Ecx, 2, CPUCAP_UMIP);
    s_MapBit(r.Ecx, 17, CPUCAP_PCID);

    s_MapBit(r.Edx, 26, CPUCAP_IBRS);
    s_MapBit(r.Edx, 27, CPUCAP_STIBP);
    s_MapBit(r.Edx, 31, CPUCAP_SSBD);
}

static void s_DetectExtendedLeaves(u32 maxExt)
{
    if (maxExt < 0x80000001)
        return;

    Arch_CpuidRegs r;
    X86_64_CpuidQuery(0x80000001, 0, &r);

    s_MapBit(r.Edx, 20, CPUCAP_NX);
    s_MapBit(r.Edx, 26, CPUCAP_1GB_PAGES);

    s_MapBit(r.Ecx, 2, CPUCAP_SVM);

    if (maxExt >= 0x80000007)
    {
        X86_64_CpuidQuery(0x80000007, 0, &r);

        s_MapBit(r.Edx, 8, CPUCAP_TSC_INVARIANT);
    }
}

void Arch_CpuCapInit(void)
{
    Arch_CpuidRegs leaf0;
    X86_64_CpuidQuery(0, 0, &leaf0);
    u32 maxLeaf = leaf0.Eax;

    u32 maxExt = X86_64_CpuidMaxExtLeaf();

    s_DetectVendor(&leaf0);
    s_DetectBrand(maxExt);
    s_DetectStdFeatures(maxLeaf);
    s_DetectExtFeatures(maxLeaf);
    s_DetectExtendedLeaves(maxExt);
}

bool Arch_CpuHasCap(Arch_CpuCap cap)
{
    if (cap >= CPUCAP__COUNT)
        return false;

    return (s_CapBitmap[cap / 32] & (1UL << (cap % 32))) != 0;
}

Arch_CpuVendor Arch_CpuGetVendor(void)
{
    return s_Vendor;
}

const char *Arch_CpuGetBrandString(void)
{
    return s_Brand;
}
