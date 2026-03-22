#ifndef SUPERVISOR_HAL_HAL_H
#define SUPERVISOR_HAL_HAL_H

#define x86_64 1

[[noreturn]] void Hal_HaltCatchFire(void);
[[noreturn]] void Hal_DefaultInterruptHandler(void);

void Hal_InitializeEarly(void);

#endif /* SUPERVISOR_HAL_HAL_H */
