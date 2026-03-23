#ifndef SUPERVISOR_ARCH_PANIC_H
#define SUPERVISOR_ARCH_PANIC_H

#include <arch/RegisterFrame.h>
#include <debug/DbgPrint.h>

void Arch_PanicFreeze(void);

void Arch_PanicDumpRegisters(Dbg_FmtContext *ctx, const Arch_RegisterFrame *frame);
void Arch_PanicBacktrace(Dbg_FmtContext *ctx, const Arch_RegisterFrame *frame, usize maxFrames);

#endif /* SUPERVISOR_ARCH_PANIC_H */
