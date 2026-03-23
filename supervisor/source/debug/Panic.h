#ifndef SUPERVISOR_PANIC_PANIC_H
#define SUPERVISOR_PANIC_PANIC_H

#include <arch/RegisterFrame.h>
#include <debug/DbgPrint.h>

typedef void (*Panic_Hook)(const char *message, const Arch_RegisterFrame *frame);

bool Panic_RegisterHook(Panic_Hook hook);

[[noreturn]] void Panic_WithFrame(const Arch_RegisterFrame *frame, const char *fmt, ...);

[[noreturn]] void Panic_(const char *fmt, ...);
[[noreturn]] void Panic_Fault(const Arch_RegisterFrame *frame, const char *reason);

#define Panic(fmt, ...) Panic_(fmt " (%s:%d)", ##__VA_ARGS__, __FILE_NAME__, __LINE__)

#define ASSERT(cond)                                                                                                   \
    ({                                                                                                                 \
        if (!(cond))                                                                                                   \
            Panic("assertion failed: %s", #cond);                                                                      \
    })

#endif /* SUPERVISOR_PANIC_PANIC_H */
