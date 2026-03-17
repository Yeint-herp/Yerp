#include <arch/CpuCap.h>
#include <arch/CpuHint.h>
#include <arch/Irq.h>
#include <arch/Rng.h>

#define RDRAND_MAX_RETRIES 10
#define RDSEED_MAX_RETRIES 32

static Arch_RngSource s_Source;

static bool s_Rdseed64(u64 *out)
{
    u8 ok;
    __asm__ volatile("rdseed %0; setc %1" : "=r"(*out), "=qm"(ok) : : "cc");
    return ok;
}

static bool s_Rdrand64(u64 *out)
{
    u8 ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*out), "=qm"(ok) : : "cc");
    return ok;
}

static u64 s_Rdtsc(void)
{
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static bool s_TryRdseed64(u64 *out)
{
    for (u32 i = 0; i < RDSEED_MAX_RETRIES; i++)
    {
        if (s_Rdseed64(out))
            return true;

        Arch_CpuRelax();
    }

    return false;
}

static bool s_TryRdrand64(u64 *out)
{
    for (u32 i = 0; i < RDRAND_MAX_RETRIES; i++)
    {
        if (s_Rdrand64(out))
            return true;

        Arch_CpuRelax();
    }

    return false;
}

static u64 s_MixTsc(void)
{
    u64 acc = s_Rdtsc();

    for (u32 i = 0; i < 8; i++)
    {
        volatile const u64 randomTimeout = acc % 100;
        for (volatile u32 delay = 0; delay < randomTimeout; delay++)
            ;

        u64 sample = s_Rdtsc();
        acc ^= sample;
        acc ^= acc << 13;
        acc ^= acc >> 7;
        acc ^= acc << 17;
    }

    return acc;
}

static bool s_Generate64_RdseedRdrand(u64 *out)
{
    if (s_TryRdseed64(out))
        return true;

    return s_TryRdrand64(out);
}

static bool s_Generate64(u64 *out)
{
    switch (s_Source)
    {
        case RNG_SRC_RDSEED:
            return s_Generate64_RdseedRdrand(out);

        case RNG_SRC_RDRAND:
            return s_TryRdrand64(out);

        case RNG_SRC_TSC:
            *out = s_MixTsc();
            return true;

        case RNG_SRC_NONE:
        default:
            return false;
    }
}

void Arch_RngInit(void)
{
    if (Arch_CpuHasCap(CPUCAP_RDSEED))
        s_Source = RNG_SRC_RDSEED;
    else if (Arch_CpuHasCap(CPUCAP_RDRAND))
        s_Source = RNG_SRC_RDRAND;
    else if (Arch_CpuHasCap(CPUCAP_TSC))
        s_Source = RNG_SRC_TSC;
    else
        s_Source = RNG_SRC_NONE;

    /// verify that advertised instructions actually work.
    if (s_Source == RNG_SRC_RDSEED || s_Source == RNG_SRC_RDRAND)
    {
        u64 test;
        if (!s_Generate64(&test))
        {
            if (Arch_CpuHasCap(CPUCAP_TSC))
                s_Source = RNG_SRC_TSC;
            else
                s_Source = RNG_SRC_NONE;
        }
    }
}

Arch_RngSource Arch_RngGetSource(void)
{
    return s_Source;
}

bool Arch_RngFill(void *buf, usize len)
{
    if (s_Source == RNG_SRC_NONE)
        return false;

    u8 *dst = buf;

    while (len >= 8)
    {
        u64 val;
        if (!s_Generate64(&val))
            return false;

        dst[0] = val;
        dst[1] = val >> 8;
        dst[2] = val >> 16;
        dst[3] = val >> 24;
        dst[4] = val >> 32;
        dst[5] = val >> 40;
        dst[6] = val >> 48;
        dst[7] = val >> 56;

        dst += 8;
        len -= 8;
    }

    if (len > 0)
    {
        u64 val;
        if (!s_Generate64(&val))
            return false;

        for (usize i = 0; i < len; i++)
            dst[i] = val >> (i * 8);
    }

    return true;
}

u64 Arch_RngU64(void)
{
    u64 val = 0;
    s_Generate64(&val);

    return val;
}
