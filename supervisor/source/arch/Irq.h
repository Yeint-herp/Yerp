#ifndef SUPERVISOR_ARCH_IRQ_H
#define SUPERVISOR_ARCH_IRQ_H

typedef usize Arch_IrqFlags;

Arch_IrqFlags Arch_IrqSave(void);
void          Arch_IrqRestore(Arch_IrqFlags flags);
bool          Arch_IrqEnabled(void);
void          Arch_IrqEnable(void);

#endif /* SUPERVISOR_ARCH_IRQ_H */
