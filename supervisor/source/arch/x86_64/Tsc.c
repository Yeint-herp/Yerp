#define DBG_MODULE "Tsc"

#include <arch/CpuHint.h>
#include <arch/x86_64/Cpuid.h>
#include <arch/x86_64/Hpet.h>
#include <arch/x86_64/LocalApic.h>
#include <arch/x86_64/Tsc.h>
#include <debug/DbgPrint.h>

static X86_64_TscInfo s_TscInfo;

static bool s_DetectTscAvailable(void)
{
    Arch_CpuidRegs regs;
    X86_64_CpuidQuery(1, 0, &regs);
    return (regs.Edx & (1u << 4)) != 0;
}

static bool s_DetectInvariantTsc(void)
{
    if (X86_64_CpuidMaxExtLeaf() < 0x80000007)
        return false;

    Arch_CpuidRegs regs;
    X86_64_CpuidQuery(0x80000007, 0, &regs);
    return (regs.Edx & (1u << 8)) != 0;
}

static u64 s_CalibrateViaCpuid(void)
{
    if (X86_64_CpuidMaxLeaf() < 0x15)
        return 0;

    Arch_CpuidRegs regs;
    X86_64_CpuidQuery(0x15, 0, &regs);

    const u32 denom     = regs.Eax;
    const u32 numer     = regs.Ebx;
    const u32 crystalHz = regs.Ecx;

    if (denom == 0 || numer == 0)
        return 0;

    if (crystalHz != 0)
    {
        u64 freq = (crystalHz * numer) / denom;
        Log(TRACE, "CPUID.15h: crystal %u Hz, ratio %u/%u -> TSC %llu Hz", crystalHz, numer, denom, freq);
        return freq;
    }

    if (X86_64_CpuidMaxLeaf() < 0x16)
        return 0;

    X86_64_CpuidQuery(0x16, 0, &regs);
    const u32 baseMhz = regs.Eax & 0xFFFF;

    if (baseMhz == 0)
        return 0;

    const u64 freq = baseMhz * 1000000ULL;
    Log(TRACE, "CPUID.16h: base freq %u MHz -> TSC ~%llu Hz", baseMhz, freq);
    return freq;
}

static u64 s_CalibrateViaHpet(void)
{
    const u64 intervalUs = 10000;
    const u32 samples    = 5;
    u64       totalTsc   = 0;

    for (u32 i = 0; i < samples; i++)
    {
        u64 tscStart = X86_64_TscRead();
        X86_64_HpetSpinWaitUs(intervalUs);
        u64 tscEnd = X86_64_TscRead();

        totalTsc += (tscEnd - tscStart);
    }

    const u64 totalUs = samples * intervalUs;
    const u64 freqHz  = (totalTsc / totalUs) * 1000000ULL;

    Log(TRACE, "HPET calibration: %llu TSC ticks over %llu us -> %llu Hz", totalTsc, totalUs, freqHz);

    return freqHz;
}

static u64 s_CalibrateViaLapic(void)
{
    const X86_64_LocalApicTimerCal *cal        = X86_64_LocalApicGetTimerCal();
    const u64                       waitUs     = 10000;
    const u32                       lapicTicks = waitUs * cal->TicksPerUs;

    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_DIVIDE, cal->Divider);
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED);
    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, lapicTicks);

    const u64 tscStart = X86_64_TscRead();

    while (X86_64_LocalApicTimerReadCurrent() > 0)
        Arch_CpuRelax();

    const u64 tscEnd = X86_64_TscRead();

    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, 0);

    const u64 tscElapsed = tscEnd - tscStart;
    const u64 freqHz     = (tscElapsed / waitUs) * 1000000ULL;

    Log(TRACE, "LAPIC calibration: %llu TSC ticks over %llu us -> %llu Hz", tscElapsed, waitUs, freqHz);

    return freqHz;
}

void X86_64_TscInit(void)
{
    s_TscInfo.IsAvailable = s_DetectTscAvailable();

    if (!s_TscInfo.IsAvailable)
    {
        Log(WARN, "TSC not available on this CPU");
        return;
    }
    s_TscInfo.IsInvariant = s_DetectInvariantTsc();

    u64 freq = s_CalibrateViaCpuid();

    if (freq == 0)
        freq = s_CalibrateViaHpet();

    if (freq == 0)
        freq = s_CalibrateViaLapic();

    if (freq == 0)
    {
        Log(ERROR, "all TSC calibration strategies failed");
        s_TscInfo.IsAvailable = false;
        return;
    }

    s_TscInfo.FreqHz  = freq;
    s_TscInfo.FreqKhz = freq / 1000;

    Log(INFO, "TSC: %llu Hz (%u kHz), invariant: %s", s_TscInfo.FreqHz, s_TscInfo.FreqKhz,
        s_TscInfo.IsInvariant ? "yes" : "no");
}

const X86_64_TscInfo *X86_64_TscGetInfo(void)
{
    return &s_TscInfo;
}

u64 X86_64_TscTicksToNs(u64 ticks)
{
    unsigned __int128 product = (unsigned __int128)ticks * 1000000000ULL;
    return product / s_TscInfo.FreqHz;
}

u64 X86_64_TscNsToTicks(u64 ns)
{
    unsigned __int128 product = (unsigned __int128)ns * s_TscInfo.FreqHz;
    return product / 1000000000ULL;
}

u64 X86_64_TscTicksToUs(const X86_64_TscInfo *info, u64 ticks)
{
    return ticks / (info->FreqHz / 1000000);
}

u64 X86_64_TscUsToTicks(const X86_64_TscInfo *info, u64 us)
{
    return us * (info->FreqHz / 1000000);
}

u64 X86_64_TscRead(void)
{
    u32 lo, hi;
    __asm__ volatile("lfence\n\t"
                     "rdtsc"
                     : "=a"(lo), "=d"(hi));

    return ((u64)hi << 32) | lo;
}

u64 X86_64_TscReadSerializing(u32 *auxOut)
{
    u32 lo, hi, aux;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    if (auxOut)
        *auxOut = aux;

    return ((u64)hi << 32) | lo;
}
