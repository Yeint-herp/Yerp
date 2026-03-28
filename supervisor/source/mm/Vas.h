#ifndef SUPERVISOR_MM_VAS_H
#define SUPERVISOR_MM_VAS_H

#include <arch/Atomic.h>
#include <core/Spinlock.h>
#include <dsa/Avl.h>
#include <mm/Vad.h>

typedef struct Mm_AddressSpace
{
    Dsa_AvlTree   VadTree;
    Core_Spinlock Lock;

    uptr LowestAddress;
    uptr HighestAddress;

    uptr PageTableRoot;

    Arch_Atomic32 ReferenceCount;
} Mm_AddressSpace;

void             Mm_SupervisorVasInit(void);
Mm_AddressSpace *Mm_GetSupervisorVas(void);

void Mm_VasInit(Mm_AddressSpace *vas, uptr lowest, uptr highest, uptr root);
void Mm_VasDestroy(Mm_AddressSpace *vas);

uptr Mm_VasAllocateRegion(Mm_AddressSpace *vas, uptr hint, usize size, Mm_VadType type, u32 prot, u32 flags);
bool Mm_VasFreeRegion(Mm_AddressSpace *vas, uptr baseAddress);

Mm_Vad *Mm_VasFindVad(Mm_AddressSpace *vas, uptr address);
bool    Mm_VasQueryVad(Mm_AddressSpace *vas, uptr address, Mm_VadInfo *out);

uptr Mm_MapIoSpace(uptr physBase, usize size, Mm_CacheType cacheType);
void Mm_UnmapIoSpace(uptr virtualAddr);

void Mm_VasReference(Mm_AddressSpace *vas);
bool Mm_VasDereference(Mm_AddressSpace *vas);

#endif /* SUPERVISOR_MM_VAS_H */
