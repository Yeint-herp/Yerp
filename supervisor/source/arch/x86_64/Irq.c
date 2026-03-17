#include <arch/Irq.h>

Arch_IrqFlags Arch_IrqSave(void)
{
    Arch_IrqFlags flags;
    __asm__ volatile("pushfq\n\t"
                     "pop %0\n\t"
                     "cli"
                     : "=r"(flags)
                     :
                     : "memory");

    return flags & (1 << 9);
}

void Arch_IrqRestore(Arch_IrqFlags flags)
{
    if (flags)
        __asm__ volatile("sti" ::: "memory");
    else
        __asm__ volatile("cli" ::: "memory");
}

bool Arch_IrqEnabled(void)
{
    Arch_IrqFlags flags;
    __asm__ volatile("pushfq\n\t"
                     "pop %0"
                     : "=r"(flags)
                     :
                     : "memory");

    return (flags & (1 << 9)) != 0;
}
