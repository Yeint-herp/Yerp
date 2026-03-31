#include <arch/CoreLocal.h>
#include <arch/Irql.h>
#include <core/Spcb.h>

static void s_WriteCr8(Irql_t irql)
{
    __asm__ volatile("mov %0, %%cr8" : : "r"((u64)irql) : "memory");
}

Irql_t Irql_Raise(Irql_t newIrql)
{
    Irql_t old = Arch_ReadLocal(CurrentIrql);
    s_WriteCr8(newIrql);
    Arch_WriteLocal(CurrentIrql, newIrql);

    return old;
}

void Irql_Lower(Irql_t oldIrql)
{
    s_WriteCr8(oldIrql);
    Arch_WriteLocal(CurrentIrql, oldIrql);
}

Irql_t Irql_GetCurrent(void)
{
    return Arch_ReadLocal(CurrentIrql);
}
