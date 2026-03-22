#include <arch/x86_64/Msr.h>

u64 X86_64_ReadMsr(u32 msr)
{
    u32 low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

    return ((u64)high << 32) | low;
}

void X86_64_WriteMsr(u32 msr, u64 value)
{
    u32 low  = value;
    u32 high = value >> 32;

    __asm__ volatile("wrmsr" ::"a"(low), "d"(high), "c"(msr));
}
