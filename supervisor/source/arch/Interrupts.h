#ifndef SUPERVISOR_ARCH_INTERRUPTS_H
#define SUPERVISOR_ARCH_INTERRUPTS_H

#include <arch/Irql.h>
#include <arch/RegisterFrame.h>

typedef u32 Interrupt_Handle;

#define INTERRUPT_HANDLE_NULL ((Interrupt_Handle) - 1)

typedef void (*Interrupt_Handler)(Arch_RegisterFrame *frame, void *context);

Interrupt_Handle Interrupt_Allocate(Irql_t irql);
void             Interrupt_Free(Interrupt_Handle handle);

bool Interrupt_Register(Interrupt_Handle handle, Interrupt_Handler handler, void *context);
bool Interrupt_Deregister(Interrupt_Handle handle);

bool Interrupt_RegisterGlobal(Interrupt_Handle handle, Interrupt_Handler handler, void *context);
bool Interrupt_DeregisterGlobal(Interrupt_Handle handle);

Irql_t Interrupt_GetIrql(Interrupt_Handle handle);

#endif /* SUPERVISOR_ARCH_INTERRUPTS_H */
