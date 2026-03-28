#define DBG_MODULE "MmSlab"

#include <arch/Atomic.h>
#include <arch/CoreLocal.h>
#include <arch/Irq.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/Early.h>
#include <mm/PfnDb.h>
#include <mm/Pool.h>
#include <mm/Slab.h>

#define SLAB_CAS_MAX_ATTEMPTS 4

static Mm_SlabCache *s_CacheList     = nullptr;
static Core_Spinlock s_CacheListLock = {};

constexpr usize s_CpuStateAlign = alignof(Mm_SlabCpuState);
static_assert(s_CpuStateAlign > 0 && (s_CpuStateAlign & (s_CpuStateAlign - 1)) == 0);

static void *s_AllocCpuSlabs(u32 cpuCount)
{
    usize size  = cpuCount * sizeof(Mm_SlabCpuState);
    usize align = alignof(Mm_SlabCpuState);

    return Mm_PermanentAllocate(size, align);
}

static void **s_FreePtr(Mm_SlabCache *cache, void *obj)
{
    return (void **)((uptr)obj + cache->FreeOffset);
}

static void *s_SlabBase(uptr pfn)
{
    return Mm_PhysToVirt(pfn << PAGE_SHIFT);
}

static uptr s_PtrToSlabPfn(void *ptr)
{
    return Mm_VirtToPhys(ptr) >> PAGE_SHIFT;
}

static Mm_SlabCpuState *s_GetCpuState(Mm_SlabCache *cache)
{
    u32 cpu = Arch_GetCurrentSpcb()->ProcessorNumber;
    return &cache->CpuSlabs[cpu];
}

static u32 s_LoadTid(void)
{
    return Arch_AtomicLoad32(&Arch_GetCurrentSpcb()->SlabTid);
}

static uptr s_AllocSlab(Mm_SlabCache *cache)
{
    uptr pfn = Mm_AllocatePages(MM_ALLOC_ZEROED, 1);
    if (pfn == (u32)MM_PFN_NULL)
        return MM_PFN_NULL;

    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    entry->e1.SlabPage         = 1;
    entry->u2.Slab.Cache       = cache;
    entry->u3.Slab.InUse       = 0;
    entry->u3.Slab.Total       = cache->ObjectsPerSlab;
    entry->u3.Slab.ObjSize     = cache->PaddedSize;
    entry->u3.Slab.Frozen      = 1;
    entry->u4.Slab.PartialNext = (u32)MM_PFN_NULL;
    entry->u4.Slab.PartialPrev = (u32)MM_PFN_NULL;

    void *base = s_SlabBase(pfn);
    void *head = nullptr;

    for (isize i = (isize)cache->ObjectsPerSlab - 1; i >= 0; i--)
    {
        void *obj              = (void *)((uptr)base + (usize)i * cache->PaddedSize);
        *s_FreePtr(cache, obj) = head;
        head                   = obj;
    }

    entry->u1.Slab.FreeList = head;

    return pfn;
}

static void s_FreeSlab(Mm_SlabCache *cache, uptr pfn)
{
    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    Core_SpinlockAcquire(&entry->Lock);
    entry->e1.SlabPage      = 0;
    entry->u1.Slab.FreeList = nullptr;
    entry->u2.Slab.Cache    = nullptr;
    Core_SpinlockRelease(&entry->Lock);

    Mm_FreePages(pfn, 1);
    cache->TotalSlabs--;
}

static void s_AddPartial(Mm_SlabCache *cache, uptr pfn)
{
    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    entry->u3.Slab.Frozen      = 0;
    entry->u4.Slab.PartialNext = cache->PartialHead;
    entry->u4.Slab.PartialPrev = (u32)MM_PFN_NULL;

    if (cache->PartialHead != (u32)MM_PFN_NULL)
    {
        Mm_Pfn *oldHead              = Mm_GetPfnEntry(cache->PartialHead);
        oldHead->u4.Slab.PartialPrev = pfn;
    }

    cache->PartialHead = pfn;
    cache->PartialCount++;
}

static void s_RemovePartial(Mm_SlabCache *cache, uptr pfn)
{
    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);
    u32     next  = entry->u4.Slab.PartialNext;
    u32     prev  = entry->u4.Slab.PartialPrev;

    if (prev != (u32)MM_PFN_NULL)
        Mm_GetPfnEntry(prev)->u4.Slab.PartialNext = next;
    else
        cache->PartialHead = next;

    if (next != (u32)MM_PFN_NULL)
        Mm_GetPfnEntry(next)->u4.Slab.PartialPrev = prev;

    entry->u4.Slab.PartialNext = (u32)MM_PFN_NULL;
    entry->u4.Slab.PartialPrev = (u32)MM_PFN_NULL;
    cache->PartialCount--;
}

static void s_DeactivateSlab(Mm_SlabCache *cache, Mm_SlabCpuState *cs)
{
    uptr pfn = cs->Slab;
    if (pfn == MM_PFN_NULL)
        return;

    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    void *head = cs->FreeList;
    if (head)
    {
        void *tail = head;
        while (*s_FreePtr(cache, tail))
            tail = *s_FreePtr(cache, tail);

        *s_FreePtr(cache, tail) = entry->u1.Slab.FreeList;
        entry->u1.Slab.FreeList = head;
    }

    cs->Slab     = MM_PFN_NULL;
    cs->FreeList = nullptr;

    Core_SpinlockAcquire(&cache->Lock);

    if (entry->u3.Slab.InUse == 0)
    {
        Core_SpinlockRelease(&cache->Lock);
        s_FreeSlab(cache, pfn);
        return;
    }

    if (entry->u1.Slab.FreeList)
        s_AddPartial(cache, pfn);
    else
        entry->u3.Slab.Frozen = 0;

    Core_SpinlockRelease(&cache->Lock);
}

static void *s_AllocSlowPath(Mm_SlabCache *cache, Mm_SlabCpuState *cs)
{
    if (cs->Slab != (u32)MM_PFN_NULL)
        s_DeactivateSlab(cache, cs);

    Core_SpinlockAcquire(&cache->Lock);

    if (cache->PartialHead != (u32)MM_PFN_NULL)
    {
        uptr pfn = cache->PartialHead;
        s_RemovePartial(cache, pfn);

        Mm_Pfn *entry         = Mm_GetPfnEntry(pfn);
        entry->u3.Slab.Frozen = 1;

        cs->Slab                = pfn;
        cs->FreeList            = entry->u1.Slab.FreeList;
        entry->u1.Slab.FreeList = nullptr;

        Core_SpinlockRelease(&cache->Lock);

        void *obj    = cs->FreeList;
        cs->FreeList = *s_FreePtr(cache, obj);
        entry->u3.Slab.InUse++;

        return obj;
    }

    Core_SpinlockRelease(&cache->Lock);

    uptr pfn = s_AllocSlab(cache);
    if (pfn == MM_PFN_NULL)
        return nullptr;

    cache->TotalSlabs++;

    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    cs->Slab                = pfn;
    cs->FreeList            = entry->u1.Slab.FreeList;
    entry->u1.Slab.FreeList = nullptr;

    void *obj    = cs->FreeList;
    cs->FreeList = *s_FreePtr(cache, obj);
    entry->u3.Slab.InUse++;

    return obj;
}

static void s_FreeSlowPath(Mm_SlabCache *cache, uptr pfn, void *obj)
{
    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    Core_SpinlockAcquire(&cache->Lock);

    bool wasFull = (entry->u1.Slab.FreeList == nullptr) && !entry->u3.Slab.Frozen;

    *s_FreePtr(cache, obj)  = entry->u1.Slab.FreeList;
    entry->u1.Slab.FreeList = obj;
    entry->u3.Slab.InUse--;

    if (entry->u3.Slab.Frozen)
    {
        Core_SpinlockRelease(&cache->Lock);
        return;
    }

    if (entry->u3.Slab.InUse == 0)
    {
        if (!wasFull)
            s_RemovePartial(cache, pfn);

        Core_SpinlockRelease(&cache->Lock);
        s_FreeSlab(cache, pfn);
        return;
    }

    if (wasFull)
        s_AddPartial(cache, pfn);

    Core_SpinlockRelease(&cache->Lock);
}

void Mm_SlabCacheInit(Mm_SlabCache *cache, const char *name, usize objSize, usize alignment)
{
    if (alignment < sizeof(void *))
        alignment = sizeof(void *);

    ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0);

    usize padded = AlignUp(objSize, alignment);
    if (padded < sizeof(void *) * 2)
        padded = sizeof(void *) * 2;

    cache->Name           = name;
    cache->ObjSize        = objSize;
    cache->PaddedSize     = padded;
    cache->Alignment      = alignment;
    cache->FreeOffset     = 0;
    cache->ObjectsPerSlab = PAGE_SIZE / padded;
    cache->PartialHead    = (u32)MM_PFN_NULL;
    cache->PartialCount   = 0;
    cache->TotalSlabs     = 0;

    ASSERT(cache->ObjectsPerSlab >= 1);

    cache->CpuCount = Core_GetProcessorCount();
    cache->CpuSlabs = s_AllocCpuSlabs(cache->CpuCount);

    for (u32 i = 0; i < cache->CpuCount; i++)
    {
        cache->CpuSlabs[i].FreeList    = nullptr;
        cache->CpuSlabs[i].Slab        = (u32)MM_PFN_NULL;
        cache->CpuSlabs[i].LocalAllocs = 0;
        cache->CpuSlabs[i].LocalFrees  = 0;
    }

    Core_SpinlockInit(&cache->Lock);

    Core_SpinlockAcquire(&s_CacheListLock);
    cache->Next = s_CacheList;
    s_CacheList = cache;
    Core_SpinlockRelease(&s_CacheListLock);

    Log(INFO, "cache '%s': objSize = %zu padded = %zu perSlab = %zu", name, objSize, padded, cache->ObjectsPerSlab);
}

void *Mm_SlabAlloc(Mm_SlabCache *cache, u32 tag)
{
    (void)tag;

    for (u32 attempt = 0; attempt < SLAB_CAS_MAX_ATTEMPTS; attempt++)
    {
        u32 tid = s_LoadTid();
        Arch_CompilerBarrier();
        u32              cpu = Arch_GetCurrentSpcb()->ProcessorNumber;
        Mm_SlabCpuState *cs  = &cache->CpuSlabs[cpu];

        if (cs->Slab == (u32)MM_PFN_NULL)
            break;

        void *obj = cs->FreeList;

        if (!obj)
            break;

        void *next = *s_FreePtr(cache, obj);

        if (!Arch_AtomicCompareExchange64((Arch_Atomic64 *)&cs->FreeList, (usize)obj, (usize)next))
            continue;

        if (s_LoadTid() != tid)
        {
            for (;;)
            {
                void *head             = cs->FreeList;
                *s_FreePtr(cache, obj) = head;
                if (Arch_AtomicCompareExchange64((Arch_Atomic64 *)&cs->FreeList, (usize)head, (usize)obj))
                    break;
            }
            continue;
        }

        Mm_GetPfnEntry(cs->Slab)->u3.Slab.InUse++;
        cs->LocalAllocs++;
        return obj;
    }

    Arch_IrqFlags    irq = Arch_IrqSave();
    Mm_SlabCpuState *cs  = s_GetCpuState(cache);

    if (cs->Slab != (u32)MM_PFN_NULL)
    {
        Mm_Pfn *entry = Mm_GetPfnEntry(cs->Slab);

        if (entry->u1.Slab.FreeList)
        {
            cs->FreeList            = entry->u1.Slab.FreeList;
            entry->u1.Slab.FreeList = nullptr;

            void *obj    = cs->FreeList;
            cs->FreeList = *s_FreePtr(cache, obj);
            entry->u3.Slab.InUse++;
            cs->LocalAllocs++;

            Arch_IrqRestore(irq);
            return obj;
        }
    }

    void *obj = s_AllocSlowPath(cache, cs);
    if (obj)
        cs->LocalAllocs++;

    Arch_IrqRestore(irq);
    return obj;
}

void Mm_SlabFree(void *ptr)
{
    if (!ptr)
        return;

    uptr    pfn   = s_PtrToSlabPfn(ptr);
    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    ASSERT(entry->e1.SlabPage);

    Mm_SlabCache *cache = entry->u2.Slab.Cache;

    for (u32 attempt = 0; attempt < SLAB_CAS_MAX_ATTEMPTS; attempt++)
    {
        u32 tid = s_LoadTid();
        Arch_CompilerBarrier();
        u32              cpu = Arch_GetCurrentSpcb()->ProcessorNumber;
        Mm_SlabCpuState *cs  = &cache->CpuSlabs[cpu];

        if (pfn != cs->Slab)
            break;

        void *head             = cs->FreeList;
        *s_FreePtr(cache, ptr) = head;

        if (!Arch_AtomicCompareExchange64((Arch_Atomic64 *)&cs->FreeList, (usize)head, (usize)ptr))
            continue;

        if (s_LoadTid() != tid)
        {
            Core_SpinlockAcquire(&cache->Lock);
            entry->u3.Slab.InUse--;
            Core_SpinlockRelease(&cache->Lock);

            Arch_IrqFlags irq = Arch_IrqSave();
            s_GetCpuState(cache)->LocalFrees++;
            Arch_IrqRestore(irq);
            return;
        }

        entry->u3.Slab.InUse--;
        cs->LocalFrees++;
        return;
    }

    Arch_IrqFlags    irq = Arch_IrqSave();
    Mm_SlabCpuState *cs  = s_GetCpuState(cache);
    cs->LocalFrees++;
    Arch_IrqRestore(irq);

    s_FreeSlowPath(cache, pfn, ptr);
}

void Mm_SlabReap(void)
{
    Core_SpinlockAcquire(&s_CacheListLock);

    for (Mm_SlabCache *cache = s_CacheList; cache; cache = cache->Next)
    {
        Core_SpinlockAcquire(&cache->Lock);

        u32 pfn = cache->PartialHead;
        while (pfn != (u32)MM_PFN_NULL)
        {
            Mm_Pfn *entry = Mm_GetPfnEntry(pfn);
            u32     next  = entry->u4.Slab.PartialNext;

            if (entry->u3.Slab.InUse == 0)
            {
                s_RemovePartial(cache, pfn);

                Core_SpinlockRelease(&cache->Lock);
                s_FreeSlab(cache, pfn);
                Core_SpinlockAcquire(&cache->Lock);
            }

            pfn = next;
        }

        Core_SpinlockRelease(&cache->Lock);
    }

    Core_SpinlockRelease(&s_CacheListLock);
}

void Mm_SlabDumpStats(void)
{
    Core_SpinlockAcquire(&s_CacheListLock);

    for (Mm_SlabCache *cache = s_CacheList; cache; cache = cache->Next)
    {
        u64 allocs = 0, frees = 0;
        for (u32 i = 0; i < cache->CpuCount; i++)
        {
            allocs += cache->CpuSlabs[i].LocalAllocs;
            frees += cache->CpuSlabs[i].LocalFrees;
        }

        Log(INFO,
            "  %-16s obj = %-5zu pad = %-5zu slabs = %-4zu partial = %-4zu "
            "alloc = %-8llu free = %-8llu",
            cache->Name, cache->ObjSize, cache->PaddedSize, cache->TotalSlabs, cache->PartialCount, allocs, frees);
    }

    Core_SpinlockRelease(&s_CacheListLock);
}
