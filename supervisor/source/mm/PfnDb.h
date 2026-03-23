#ifndef SUPERVISOR_MM_PFNDB_H
#define SUPERVISOR_MM_PFNDB_H

#include <core/Spinlock.h>
#include <mm/PageTable.h>

#define MM_PFN_NULL ((uptr) - 1)
#define PAGE_SHIFT  12
#define PAGE_SIZE   (1ULL << PAGE_SHIFT)

#define MM_ALLOC_ANY 0U

#define MM_ALLOC_ZEROED     (1U << 0)
#define MM_ALLOC_CONTIGUOUS (1U << 1)
#define MM_ALLOC_BELOW_4G   (1U << 2)
#define MM_ALLOC_BELOW_16M  (1U << 3)

typedef struct [[gnu::aligned(64)]] Mm_Pfn
{
    union
    {
        uptr               Flink;
        Mm_PageTableEntry *PteAddress;
    } u1;

    union
    {
        uptr Blink;
        u64  ShareCount;
    } u2;

    Mm_PageTableEntry OriginalPte;

    uptr PteFrame;

    union
    {
        struct
        {
            u64 PageLocation : 3;
            u64 Modified : 1;
            u64 PrototypePte : 1;
            u64 ReadInProgress : 1;
            u64 WriteInProgress : 1;
            u64 RemovalRequested : 1;
            u64 CacheType : 3;
            u64 MagazineCached : 1;
            u64 Reserved : 52;
        };
        u64 EntireFlags;
    } e1;

    volatile i32  ReferenceCount;
    Core_Spinlock Lock;

    u64 Reserved[2];
} Mm_Pfn;

typedef enum Mm_PageLocation
{
    ZeroedPageList      = 0,
    FreePageList        = 1,
    StandbyPageList     = 2,
    ModifiedPageList    = 3,
    ModifiedNoWriteList = 4,
    BadPageList         = 5,
    ActiveAndValid      = 6,
    TransitionPage      = 7,
} Mm_PageLocation;

typedef struct Mm_PfnList
{
    uptr Total;
    uptr Flink;
    uptr Blink;

    Mm_PageLocation ListName;

    Core_Spinlock Lock;
} Mm_PfnList;

extern Mm_PfnList Mm_ZeroedPageListHead;
extern Mm_PfnList Mm_FreePageListHead;
extern Mm_PfnList Mm_StandbyPageListHead;
extern Mm_PfnList Mm_ModifiedPageListHead;
extern Mm_PfnList Mm_BadPageListHead;

uptr Mm_RemovePageFromList(Mm_PfnList *list);

/// Caller must hold per-PFN lock.
void Mm_InsertPageInList(Mm_PfnList *list, uptr pfn);

/// Caller must NOT hold per-PFN lock.
void Mm_TransitionPage(uptr pfn, Mm_PageLocation newState);

uptr Mm_AllocatePages(u32 flags, uptr count);
void Mm_FreePages(uptr pfn, uptr count);

Mm_Pfn *Mm_GetPfnEntry(uptr pfn);

uptr Mm_GetTotalPhysicalPages(void);
uptr Mm_GetFreePageCount(void);

void Mm_PfnDbInit(void);

#endif /* SUPERVISOR_MM_PFNDB_H */
