#ifndef SUPERVISOR_ARCH_CONTEXT_H
#define SUPERVISOR_ARCH_CONTEXT_H

typedef struct Arch_ThreadContext
{
    u64 Sp;
} Arch_ThreadContext;

typedef void (*Arch_ThreadEntry)(void *parameter);

void Arch_ContextSwitch(Arch_ThreadContext *oldCtx, Arch_ThreadContext newCtx);
void Arch_ContextInit(Arch_ThreadContext *ctx, uptr stackTop, Arch_ThreadEntry entry, void *parameter);

void Arch_ThreadTrampoline(void);

#endif /* SUPERVISOR_ARCH_CONTEXT_H */
