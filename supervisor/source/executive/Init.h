#ifndef SUPERVISOR_Exec_Exec_H
#define SUPERVISOR_Exec_Exec_H

#define x86_64 1

[[noreturn]] void Exec_HaltCatchFire(void);
[[noreturn]] void Exec_DefaultInterruptHandler(void);

void Exec_InitializeEarly(void);

#endif /* SUPERVISOR_Exec_Exec_H */
