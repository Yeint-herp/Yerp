#ifndef SUPERVISOR_Ex_Ex_H
#define SUPERVISOR_Ex_Ex_H

#include <arch/RegisterFrame.h>

#define x86_64 1

[[noreturn]] void Ex_HaltCatchFire(void);
[[noreturn]] void Ex_DefaultInterruptHandler(Arch_RegisterFrame *frame);

void Ex_InitializeEarly(void);

#endif /* SUPERVISOR_Ex_Ex_H */
