#ifndef SUPERVISOR_Exec_Exec_H
#define SUPERVISOR_Exec_Exec_H

#include <arch/RegisterFrame.h>

#define x86_64 1

[[noreturn]] void Exec_HaltCatchFire(void);
[[noreturn]] void Exec_DefaultInterruptHandler(Arch_RegisterFrame *frame);

void Exec_InitializeEarly(void);

#endif /* SUPERVISOR_Exec_Exec_H */
