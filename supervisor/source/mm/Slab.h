#ifndef SUPERVISOR_MM_SLAB_H
#define SUPERVISOR_MM_SLAB_H

#include <core/Spinlock.h>

typedef struct Mm_SlabCpuState
{
    void *FreeList;
    uptr  Slab;

    usize LocalAllocs;
    usize LocalFrees;
} Mm_SlabCpuState;

typedef struct Mm_SlabCache
{
    const char *Name;
    usize       ObjSize;
    usize       PaddedSize;
    usize       Alignment;
    usize       ObjectsPerSlab;
    usize       FreeOffset;

    u32           PartialHead;
    usize         PartialCount;
    Core_Spinlock Lock;

    Mm_SlabCpuState *CpuSlabs;
    u32              CpuCount;

    Arch_Atomic64 TotalSlabs;

    struct Mm_SlabCache *Next;
} Mm_SlabCache;

void Mm_SlabCacheInit(Mm_SlabCache *cache, const char *name, usize objSize, usize alignment);

void *Mm_SlabAlloc(Mm_SlabCache *cache, u32 tag);
void  Mm_SlabFree(void *ptr);

void Mm_SlabReap(void);

void Mm_SlabSetPoolReady(void);
void Mm_SlabDumpStats(void);

#endif /* SUPERVISOR_MM_SLAB_H */
