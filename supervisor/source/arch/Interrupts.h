#ifndef SUPERVISOR_ARCH_INTERRUPTS_H
#define SUPERVISOR_ARCH_INTERRUPTS_H

#include <arch/Irql.h>
#include <arch/RegisterFrame.h>

typedef u32 Interrupt_Handle;

#define INTERRUPT_HANDLE_NULL ((Interrupt_Handle) - 1)

typedef void (*Interrupt_Handler)(Arch_RegisterFrame *frame, void *context);

void Interrupt_ControllerInit(void);

Interrupt_Handle Interrupt_Allocate(Irql_t irql);
void             Interrupt_Free(Interrupt_Handle handle);

bool Interrupt_Register(Interrupt_Handle handle, Interrupt_Handler handler, void *context);
bool Interrupt_Deregister(Interrupt_Handle handle);

bool Interrupt_RegisterGlobal(Interrupt_Handle handle, Interrupt_Handler handler, void *context);
bool Interrupt_DeregisterGlobal(Interrupt_Handle handle);

Irql_t Interrupt_GetIrql(Interrupt_Handle handle);

typedef u32 Interrupt_HwIrq;

typedef enum
{
    kInterruptPolarityDefault,
    kInterruptPolarityActiveHigh,
    kInterruptPolarityActiveLow,
} Interrupt_Polarity;

typedef enum
{
    kInterruptTriggerDefault,
    kInterruptTriggerEdge,
    kInterruptTriggerLevel,
} Interrupt_Trigger;

typedef struct
{
    Interrupt_Polarity polarity;
    Interrupt_Trigger  trigger;
} Interrupt_Flags;

Interrupt_Handle Interrupt_AllocateHwIrq(Interrupt_HwIrq hwIrq, Interrupt_Flags flags, Irql_t irql);

void Interrupt_SendEoi(Interrupt_Handle handle);
void Interrupt_Mask(Interrupt_Handle handle);
void Interrupt_Unmask(Interrupt_Handle handle);

typedef enum
{
    kIpiTargetSelf,
    kIpiTargetAll,
    kIpiTargetAllExcludingSelf,
    kIpiTargetSpecific,
} Ipi_Target;

void Interrupt_SendIpi(Ipi_Target target, u32 processorNumber, Interrupt_Handle handle);

#endif /* SUPERVISOR_ARCH_INTERRUPTS_H */
