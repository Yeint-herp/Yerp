#define DBG_MODULE "Lapic"

#include <arch/CoreLocal.h>
#include <arch/Interrupts.h>
#include <arch/Io.h>
#include <arch/MmArch.h>
#include <arch/x86_64/Apic.h>
#include <arch/x86_64/Cpuid.h>
#include <arch/x86_64/Hpet.h>
#include <arch/x86_64/LocalApic.h>
#include <arch/x86_64/Msr.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/Vas.h>

static uptr                     s_LapicMmioBase;
static bool                     s_X2ApicMode;
static X86_64_LocalApicTimerCal s_TimerCal;

u32 X86_64_LocalApicReadReg(u32 mmioOffset)
{
    if (s_X2ApicMode)
    {
        const u32 msr = X2APIC_MSR_BASE + (mmioOffset >> 4);
        return X86_64_ReadMsr(msr);
    }
    else
        return Arch_MmioRead32((void *)(s_LapicMmioBase + mmioOffset));
}

void X86_64_LocalApicWriteReg(u32 mmioOffset, u32 value)
{
    if (s_X2ApicMode)
    {
        u32 msr = X2APIC_MSR_BASE + (mmioOffset >> 4);
        X86_64_WriteMsr(msr, value);
    }
    else
        Arch_MmioWrite32((void *)(s_LapicMmioBase + mmioOffset), value);
}

void X86_64_LocalApicSendEoi(void)
{
    X86_64_LocalApicWriteReg(LAPIC_REG_EOI, 0);
}

u32 X86_64_LocalApicGetId(void)
{
    u32 raw = X86_64_LocalApicReadReg(LAPIC_REG_ID);

    return s_X2ApicMode ? raw : (raw >> 24);
}

static void s_SetupCommon(void)
{
    const X86_64_ApicState *state = X86_64_ApicGetState();

    u32 svr = X86_64_LocalApicReadReg(LAPIC_REG_SPURIOUS);
    svr |= LAPIC_SPURIOUS_ENABLE;
    svr = (svr & ~0xFFu) | 0xFF;
    X86_64_LocalApicWriteReg(LAPIC_REG_SPURIOUS, svr);

    X86_64_LocalApicWriteReg(LAPIC_REG_TPR, 0);

    u32 lint0 = LAPIC_LVT_MASKED;
    u32 lint1 = LAPIC_LVT_MASKED;

    Dsa_VectorForEach(state->Nmis, nmi)
    {
        u32 lvtVal = LAPIC_LVT_DELIV_NMI;

        if ((nmi->Flags & 0x03) == 0x03)
            lvtVal |= LAPIC_LVT_LEVEL;

        if (nmi->Lint == 0)
            lint0 = lvtVal;
        else if (nmi->Lint == 1)
            lint1 = lvtVal;
    }

    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_LINT0, lint0);
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_LINT1, lint1);

    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_ERROR, LAPIC_LVT_MASKED);
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_THERMAL, LAPIC_LVT_MASKED);
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_PERF, LAPIC_LVT_MASKED);

    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED);

    Log(TRACE, "LAPIC %u: SVR %#x, LINT0 %#x, LINT1 %#x", X86_64_LocalApicGetId(), svr, lint0, lint1);
}

void X86_64_LocalApicInit(void)
{
    const X86_64_ApicState *state = X86_64_ApicGetState();
    s_X2ApicMode                  = state->x2apic;

    if (s_X2ApicMode)
    {
        u64 apicBase = X86_64_ReadMsr(0x1B);
        apicBase |= (1ULL << 10) | (1ULL << 11);
        X86_64_WriteMsr(0x1B, apicBase);

        Log(INFO, "x2APIC mode enabled");
    }
    else
    {
        s_LapicMmioBase = Mm_MapIoSpace(state->LapicBase, 0x1000, kMmCacheUncached);
        if (!s_LapicMmioBase)
            Panic("failed to map LAPIC MMIO at phys %#llx", state->LapicBase);

        u64 apicBase = X86_64_ReadMsr(0x1B);
        apicBase |= (1ULL << 11);
        X86_64_WriteMsr(0x1B, apicBase);

        Log(INFO, "xAPIC mode, MMIO at VA %#llx (phys %#llx)", s_LapicMmioBase, state->LapicBase);
    }

    s_SetupCommon();
}

void X86_64_LocalApicInitAp(void)
{
    if (s_X2ApicMode)
    {
        u64 apicBase = X86_64_ReadMsr(0x1B);
        apicBase |= (1ULL << 10) | (1ULL << 11);
        X86_64_WriteMsr(0x1B, apicBase);
    }
    else
    {
        u64 apicBase = X86_64_ReadMsr(0x1B);
        apicBase |= (1ULL << 11);
        X86_64_WriteMsr(0x1B, apicBase);
    }

    s_SetupCommon();
}

#define CALIB_SAMPLE_COUNT 10
#define CALIB_INTERVAL_US  1000
#define CALIB_DIVIDER      LAPIC_TIMER_DIV_1
#define CALIB_INIT_COUNT   0xFFFFFFFFu

void X86_64_LocalApicCalibrateTimer(void)
{
    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_DIVIDE, CALIB_DIVIDER);

    u64 totalTicks = 0;
    for (u32 i = 0; i < CALIB_SAMPLE_COUNT; i++)
    {
        X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, CALIB_INIT_COUNT);

        X86_64_HpetSpinWaitUs(CALIB_INTERVAL_US);

        u32 remaining = X86_64_LocalApicTimerReadCurrent();
        u32 elapsed   = CALIB_INIT_COUNT - remaining;

        totalTicks += elapsed;

        X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, 0);
    }

    u64 totalUs = CALIB_SAMPLE_COUNT * CALIB_INTERVAL_US;

    s_TimerCal.TicksPerUs = totalTicks / totalUs;
    s_TimerCal.Divider    = CALIB_DIVIDER;

    if (s_TimerCal.TicksPerUs == 0)
    {
        Log(WARN, "calibration yielded 0 ticks/us");
        s_TimerCal.TicksPerUs = 1;
    }

    Log(INFO, "calibrated: %u ticks/us (divider %#x, %u samples, %llu total ticks over %llu us)", s_TimerCal.TicksPerUs,
        s_TimerCal.Divider, CALIB_SAMPLE_COUNT, totalTicks, totalUs);
}

const X86_64_LocalApicTimerCal *X86_64_LocalApicGetTimerCal(void)
{
    return &s_TimerCal;
}

void X86_64_LocalApicTimerOneShot(u8 vector, u64 us)
{
    u64 ticks = us * s_TimerCal.TicksPerUs;

    if (ticks > 0xFFFFFFFFULL)
    {
        Log(WARN, "one-shot %llu us exceeds 32-bit count", us);
        ticks = 0xFFFFFFFFULL;
    }

    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_DIVIDE, s_TimerCal.Divider);
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_ONESHOT | vector);
    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, ticks);
}

void X86_64_LocalApicTimerPeriodic(u8 vector, u64 us)
{
    u64 ticks = us * s_TimerCal.TicksPerUs;

    if (ticks > 0xFFFFFFFFULL)
    {
        Log(WARN, "periodic %llu us exceeds 32-bit count", us);
        ticks = 0xFFFFFFFFULL;
    }

    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_DIVIDE, s_TimerCal.Divider);
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | vector);
    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, ticks);
}

void X86_64_LocalApicTimerStop(void)
{
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED);
    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, 0);
}

u32 X86_64_LocalApicTimerReadCurrent(void)
{
    return X86_64_LocalApicReadReg(LAPIC_REG_TIMER_CURRENT);
}

void X86_64_LocalApicTimerOneShotTicks(u8 vector, u32 ticks)
{
    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_DIVIDE, s_TimerCal.Divider);
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_ONESHOT | vector);
    X86_64_LocalApicWriteReg(LAPIC_REG_TIMER_INIT, ticks);
}

u32 X86_64_LocalApicTimerGetRate(void)
{
    return s_TimerCal.TicksPerUs;
}

bool X86_64_LocalApicTimerHasTscDeadline(void)
{
    X86_64_CpuidRegs regs;
    X86_64_CpuidQuery(1, 0, &regs);
    return (regs.Ecx & (1u << 24)) != 0;
}

void X86_64_LocalApicTimerTscDeadline(u8 vector, u64 deadline)
{
    X86_64_LocalApicWriteReg(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_TSC_DEADLINE | vector);
    X86_64_WriteMsr(0x6E0, deadline);
}
