#ifndef SUPERVISOR_CORE_SPINLOCK_H
#define SUPERVISOR_CORE_SPINLOCK_H

#include <arch/Atomic.h>
#include <arch/Irq.h>
#include <arch/Irql.h>

#define SPINLOCK_FREE   0
#define SPINLOCK_LOCKED 1

typedef struct
{
    Arch_Atomic32 State;
} Core_Spinlock;

bool Core_SpinlockTryAcquire(Core_Spinlock *lock);
void Core_SpinlockAcquire(Core_Spinlock *lock);
void Core_SpinlockRelease(Core_Spinlock *lock);
void Core_SpinlockInit(Core_Spinlock *lock);

Arch_IrqFlags Core_SpinlockAcquireIrq(Core_Spinlock *lock);
void          Core_SpinlockReleaseIrq(Core_Spinlock *lock, Arch_IrqFlags flags);
bool          Core_SpinlockTryAcquireIrq(Core_Spinlock *lock, Arch_IrqFlags *flags);

Irql_t Core_SpinlockAcquireIrql(Core_Spinlock *lock, Irql_t irql);
void   Core_SpinlockReleaseIrql(Core_Spinlock *lock, Irql_t oldIrql);
bool   Core_SpinlockTryAcquireIrql(Core_Spinlock *lock, Irql_t irql, Irql_t *oldIrql);

#endif /* SUPERVISOR_CORE_SPINLOCK_H */
