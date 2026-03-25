#define DBG_MODULE "ExPool"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/Early.h>
#include <mm/PfnDb.h>
#include <mm/Pool.h>
#include <mm/Slab.h>

#define POOL_MIN_SHIFT    4
#define POOL_MAX_SHIFT    11
#define POOL_BUCKET_COUNT (POOL_MAX_SHIFT - POOL_MIN_SHIFT + 1)

static Mm_SlabCache s_Buckets[POOL_BUCKET_COUNT];

static const char *s_BucketNames[POOL_BUCKET_COUNT] = {
    "pool-16", "pool-32", "pool-64", "pool-128", "pool-256", "pool-512", "pool-1024", "pool-2048",
};

static inline usize s_SizeToIndex(usize size)
{
    if (size <= (1ULL << POOL_MIN_SHIFT))
        return 0;

    usize shift = (sizeof(usize) * 8) - __builtin_clzl(size - 1);
    usize index = shift - POOL_MIN_SHIFT;

    return index >= POOL_BUCKET_COUNT ? POOL_BUCKET_COUNT : index;
}

void Ex_PoolInit(void)
{
    for (usize i = 0; i < POOL_BUCKET_COUNT; i++)
    {
        usize objSize = 1ULL << (POOL_MIN_SHIFT + i);
        Mm_SlabCacheInit(&s_Buckets[i], s_BucketNames[i], objSize, sizeof(void *));
    }
    Mm_SlabSetPoolReady();

    Log(INFO, "pool initialized (%zu size classes)", POOL_BUCKET_COUNT);
}

void *Ex_Allocate(usize size, u32 tag)
{
    if (size == 0)
        return nullptr;

    usize index = s_SizeToIndex(size);

    if (index < POOL_BUCKET_COUNT)
    {
        void *ptr = Mm_SlabAlloc(&s_Buckets[index], tag);
        if (ptr)
            Core_ZeroMemory(ptr, size);

        return ptr;
    }

    usize pageCount = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

    uptr pfn = Mm_AllocatePages(MM_ALLOC_ZEROED, pageCount);
    if (pfn == MM_PFN_NULL)
        return nullptr;

    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    Core_SpinlockAcquire(&entry->Lock);
    entry->e1.LargePoolHead       = 1;
    entry->ex.Pool.LargePageCount = pageCount;
    entry->ex.Pool.LargeTag       = tag;
    Core_SpinlockRelease(&entry->Lock);

    return Mm_PhysToVirt(pfn << PAGE_SHIFT);
}

void Ex_Free(void *ptr)
{
    if (!ptr)
        return;

    uptr    pfn   = Mm_VirtToPhys(ptr) >> PAGE_SHIFT;
    Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

    ASSERT(entry != nullptr);

    if (entry->e1.SlabPage)
    {
        Mm_SlabFree(ptr);
        return;
    }

    if (entry->e1.LargePoolHead)
    {
        Core_SpinlockAcquire(&entry->Lock);
        u32 pageCount           = entry->ex.Pool.LargePageCount;
        entry->e1.LargePoolHead = 0;
        Core_SpinlockRelease(&entry->Lock);

        Mm_FreePages(pfn, pageCount);
        return;
    }

    Panic("Ex_Free: invlaid pointer %p (PFN %llu)", ptr, pfn);
}
