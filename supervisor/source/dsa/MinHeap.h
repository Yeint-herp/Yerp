#ifndef SUPERVISOR_DSA_MINHEAP_H
#define SUPERVISOR_DSA_MINHEAP_H

#include <core/Memory.h>
#include <executive/Pool.h>

typedef struct Dsa_MinHeapHeader
{
    u32   Tag;
    usize Count;
    usize Capacity;
} Dsa_MinHeapHeader;

#define minheap_of(T) T *

#define Dsa_MinHeapHdr(h)      ((Dsa_MinHeapHeader *)(h) - 1)
#define Dsa_MinHeapCount(h)    ((h) ? Dsa_MinHeapHdr(h)->Count : 0)
#define Dsa_MinHeapCapacity(h) ((h) ? Dsa_MinHeapHdr(h)->Capacity : 0)
#define Dsa_MinHeapEmpty(h)    (Dsa_MinHeapCount(h) == 0)
#define Dsa_MinHeapPeek(h)     ((h)[0])

#define Dsa_MinHeapFree(h)                                                                                             \
    ({                                                                                                                 \
        if (h)                                                                                                         \
        {                                                                                                              \
            Ex_Free(Dsa_MinHeapHdr(h));                                                                                \
            (h) = nullptr;                                                                                             \
        }                                                                                                              \
    })

#define Dsa_MinHeapClear(h)                                                                                            \
    ({                                                                                                                 \
        if (h)                                                                                                         \
            Dsa_MinHeapHdr(h)->Count = 0;                                                                              \
    })

#define Dsa_MinHeapGrow(h, tag)                                                                                        \
    ({                                                                                                                 \
        usize              __newCap = (h) ? Dsa_MinHeapHdr(h)->Capacity * 2 : 16;                                      \
        usize              __size   = sizeof(Dsa_MinHeapHeader) + __newCap * sizeof(*(h));                             \
        Dsa_MinHeapHeader *__hdr;                                                                                      \
        if (h)                                                                                                         \
        {                                                                                                              \
            __hdr  = Ex_Allocate(__size, (tag));                                                                       \
            *__hdr = *Dsa_MinHeapHdr(h);                                                                               \
            Core_CopyMemory(__hdr + 1, (h), Dsa_MinHeapHdr(h)->Count * sizeof(*(h)));                                  \
            Ex_Free(Dsa_MinHeapHdr(h));                                                                                \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            __hdr        = Ex_Allocate(__size, (tag));                                                                 \
            __hdr->Count = 0;                                                                                          \
            __hdr->Tag   = (tag);                                                                                      \
        }                                                                                                              \
        __hdr->Capacity = __newCap;                                                                                    \
        (h)             = (typeof(h))(__hdr + 1);                                                                      \
    })

#define _Dsa_MinHeapSwap(h, i, j)                                                                                      \
    ({                                                                                                                 \
        typeof(*(h)) __tmp = (h)[(i)];                                                                                 \
        (h)[(i)]           = (h)[(j)];                                                                                 \
        (h)[(j)]           = __tmp;                                                                                    \
    })

#define _Dsa_MinHeapSiftUp(h, idx, cmp)                                                                                \
    ({                                                                                                                 \
        usize __i = (idx);                                                                                             \
        while (__i > 0)                                                                                                \
        {                                                                                                              \
            usize __p = (__i - 1) / 2;                                                                                 \
            if ((cmp)((h)[__i], (h)[__p]) >= 0)                                                                        \
                break;                                                                                                 \
            _Dsa_MinHeapSwap(h, __i, __p);                                                                             \
            __i = __p;                                                                                                 \
        }                                                                                                              \
    })

#define _Dsa_MinHeapSiftDown(h, idx, count, cmp)                                                                       \
    ({                                                                                                                 \
        usize __i = (idx);                                                                                             \
        usize __n = (count);                                                                                           \
        for (;;)                                                                                                       \
        {                                                                                                              \
            usize __c = 2 * __i + 1;                                                                                   \
            if (__c >= __n)                                                                                            \
                break;                                                                                                 \
            if (__c + 1 < __n && (cmp)((h)[__c + 1], (h)[__c]) < 0)                                                    \
                __c++;                                                                                                 \
            if ((cmp)((h)[__i], (h)[__c]) <= 0)                                                                        \
                break;                                                                                                 \
            _Dsa_MinHeapSwap(h, __i, __c);                                                                             \
            __i = __c;                                                                                                 \
        }                                                                                                              \
    })

#define Dsa_MinHeapPush(h, e, cmp, tag)                                                                                \
    ({                                                                                                                 \
        if (!(h) || Dsa_MinHeapHdr(h)->Count >= Dsa_MinHeapHdr(h)->Capacity)                                           \
            Dsa_MinHeapGrow((h), (tag));                                                                               \
        usize __pos = Dsa_MinHeapHdr(h)->Count++;                                                                      \
        (h)[__pos]  = (e);                                                                                             \
        _Dsa_MinHeapSiftUp(h, __pos, cmp);                                                                             \
    })

#define Dsa_MinHeapPop(h, cmp)                                                                                         \
    ({                                                                                                                 \
        typeof(*(h)) __ret = (h)[0];                                                                                   \
        usize        __cnt = --Dsa_MinHeapHdr(h)->Count;                                                               \
        if (__cnt > 0)                                                                                                 \
        {                                                                                                              \
            (h)[0] = (h)[__cnt];                                                                                       \
            _Dsa_MinHeapSiftDown(h, 0, __cnt, cmp);                                                                    \
        }                                                                                                              \
        __ret;                                                                                                         \
    })

#define Dsa_MinHeapRemoveAt(h, idx, cmp)                                                                               \
    ({                                                                                                                 \
        usize        __ri  = (idx);                                                                                    \
        typeof(*(h)) __ret = (h)[__ri];                                                                                \
        usize        __cnt = --Dsa_MinHeapHdr(h)->Count;                                                               \
        if (__ri < __cnt)                                                                                              \
        {                                                                                                              \
            (h)[__ri] = (h)[__cnt];                                                                                    \
            _Dsa_MinHeapSiftDown(h, __ri, __cnt, cmp);                                                                 \
            _Dsa_MinHeapSiftUp(h, __ri, cmp);                                                                          \
        }                                                                                                              \
        __ret;                                                                                                         \
    })

#endif /* SUPERVISOR_DSA_MINHEAP_H */
