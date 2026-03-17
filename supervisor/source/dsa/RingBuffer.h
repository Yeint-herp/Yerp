#ifndef SUPERVISOR_DSA_RINGBUFFER_H
#define SUPERVISOR_DSA_RINGBUFFER_H

#include <arch/Atomic.h>

#define ringbuf_of(T, Size)                                                                                            \
    struct                                                                                                             \
    {                                                                                                                  \
        Arch_Atomic64 Head;                                                                                            \
        Arch_Atomic64 Tail;                                                                                            \
        T             Buffer[Size];                                                                                    \
    }

#define Dsa_RingBufferPush(rb, e)                                                                                      \
    ({                                                                                                                 \
        bool __ret       = true;                                                                                       \
        u64  currentHead = Arch_AtomicLoad64(&(rb)->Head);                                                             \
        u64  currentTail = Arch_AtomicLoad64(&(rb)->Tail);                                                             \
        if (currentHead - currentTail > ArraySize((rb)->Buffer))                                                       \
            __ret = false;                                                                                             \
        else                                                                                                           \
        {                                                                                                              \
            u64 index           = currentHead % ArraySize((rb)->Buffer);                                               \
            (rb)->Buffer[index] = e;                                                                                   \
            Arch_AtomicStore64(&(rb)->Head, currentHead + 1);                                                          \
        }                                                                                                              \
        __ret;                                                                                                         \
    })

#define Dsa_RingBufferPop(rb)                                                                                          \
    ({                                                                                                                 \
        typeof(*(rb)->Buffer) __ret       = {};                                                                        \
        u64                   currentTail = Arch_AtomicLoad64(&(rb)->Tail);                                            \
        u64                   currentHead = Arch_AtomicLoad64(&(rb)->Head);                                            \
                                                                                                                       \
        if (currentTail != currentHead)                                                                                \
        {                                                                                                              \
            u64 index = currentTail % ArraySize((rb)->Buffer);                                                         \
            __ret     = (rb)->Buffer[index];                                                                           \
            Arch_AtomicStore64(&(rb)->Tail, currentTail + 1);                                                          \
        }                                                                                                              \
        __ret;                                                                                                         \
    })

#define Dsa_RingBufferWrap(rb) Arch_AtomicStore64(&(rb)->Head, 0)

#endif /* SUPERVISOR_DSA_RINGBUFFER_H */
