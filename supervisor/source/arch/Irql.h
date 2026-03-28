#ifndef SUPERVISOR_ARCH_IRQL_H
#define SUPERVISOR_ARCH_IRQL_H

typedef u8 Irql_t;

#define IRQL_PASSIVE  ((Irql_t)0)
#define IRQL_APC      ((Irql_t)1)
#define IRQL_DISPATCH ((Irql_t)2)

#define IRQL_DIRQL_MIN ((Irql_t)3)
#define IRQL_DIRQL_MAX ((Irql_t)12)

#define IRQL_CLOCK ((Irql_t)13)
#define IRQL_IPI   ((Irql_t)14)
#define IRQL_HIGH  ((Irql_t)15)

Irql_t Irql_Raise(Irql_t newIrql);
void   Irql_Lower(Irql_t oldIrql);

Irql_t Irql_GetCurrent(void);

#endif /* SUPERVISOR_ARCH_IRQL_H */
