#ifndef SUPERVISOR_DSA_VECTOR_H
#define SUPERVISOR_DSA_VECTOR_H

#include <core/Memory.h>
#include <mm/Pool.h>

typedef struct Dsa_VectorHeader
{
    u32   Tag;
    usize Count;
    usize Capacity;
} Dsa_VectorHeader;

#define vector_of(T) T *

#define Dsa_VectorHdr(v) ((Dsa_VectorHeader *)(v) - 1)

#define Dsa_VectorCount(v)    ((v) ? Dsa_VectorHdr(v)->Count : 0)
#define Dsa_VectorCapacity(v) ((v) ? Dsa_VectorHdr(v)->Capacity : 0)

#define Dsa_VectorFree(v)                                                                                              \
    ({                                                                                                                 \
        if (v)                                                                                                         \
        {                                                                                                              \
            Ex_Free(Dsa_VectorHdr(v));                                                                                 \
            (v) = nullptr;                                                                                             \
        }                                                                                                              \
    })

#define Dsa_VectorGrow(v, tag)                                                                                         \
    ({                                                                                                                 \
        usize             __newCap = (v) ? Dsa_VectorHdr(v)->Capacity * 2 : 8;                                         \
        usize             __size   = sizeof(Dsa_VectorHeader) + __newCap * sizeof(*(v));                               \
        Dsa_VectorHeader *__hdr;                                                                                       \
        if (v)                                                                                                         \
        {                                                                                                              \
            __hdr  = Ex_Allocate(__size, (tag));                                                                       \
            *__hdr = *Dsa_VectorHdr(v);                                                                                \
            Core_CopyMemory(__hdr + 1, (v), Dsa_VectorHdr(v)->Count * sizeof(*(v)));                                   \
            Ex_Free(Dsa_VectorHdr(v));                                                                                 \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            __hdr        = Ex_Allocate(__size, (tag));                                                                 \
            __hdr->Count = 0;                                                                                          \
            __hdr->Tag   = (tag);                                                                                      \
        }                                                                                                              \
        __hdr->Capacity = __newCap;                                                                                    \
        (v)             = (typeof(v))(__hdr + 1);                                                                      \
    })

#define _Dsa_VectorPush2(v, e)                                                                                         \
    ({                                                                                                                 \
        if (!(v) || Dsa_VectorHdr(v)->Count >= Dsa_VectorHdr(v)->Capacity)                                             \
            Dsa_VectorGrow((v), (v) ? Dsa_VectorHdr(v)->Tag : EX_TAG_GENERIC);                                         \
        (v)[Dsa_VectorHdr(v)->Count++] = (e);                                                                          \
    })

#define _Dsa_VectorPush3(v, e, tag)                                                                                    \
    ({                                                                                                                 \
        if (!(v) || Dsa_VectorHdr(v)->Count >= Dsa_VectorHdr(v)->Capacity)                                             \
            Dsa_VectorGrow((v), (tag));                                                                                \
        (v)[Dsa_VectorHdr(v)->Count++] = (e);                                                                          \
    })

#define Dsa_VectorPush(...) MacroDispatch(_Dsa_VectorPush, __VA_ARGS__)

#define Dsa_VectorPop(v)                                                                                               \
    ({                                                                                                                 \
        typeof(*(v)) __ret = {};                                                                                       \
        if ((v) && Dsa_VectorHdr(v)->Count > 0)                                                                        \
            __ret = (v)[--Dsa_VectorHdr(v)->Count];                                                                    \
        __ret;                                                                                                         \
    })

#define Dsa_VectorClear(v)                                                                                             \
    ({                                                                                                                 \
        if (v)                                                                                                         \
            Dsa_VectorHdr(v)->Count = 0;                                                                               \
    })

#define Dsa_VectorLast(v) ((v)[Dsa_VectorHdr(v)->Count - 1])

#define Dsa_VectorForEach(v, it) for (typeof(*(v)) *(it) = (v); (it) < (v) + Dsa_VectorCount(v); ++(it))

#endif /* SUPERVISOR_DSA_VECTOR_H */
