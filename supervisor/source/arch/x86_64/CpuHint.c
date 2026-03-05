#include <arch/CpuHint.h>

void Arch_CpuRelax(void)
{
    __asm__ volatile("pause" ::: "memory");
}
