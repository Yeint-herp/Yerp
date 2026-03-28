#ifndef SUPERVISOR_ARCH_MMARCH_H
#define SUPERVISOR_ARCH_MMARCH_H

#define MM_PROT_READ    (1U << 0)
#define MM_PROT_WRITE   (1U << 1)
#define MM_PROT_EXECUTE (1U << 2)
#define MM_PROT_USER    (1U << 3)
#define MM_PROT_GLOBAL  (1U << 4)

typedef enum Mm_CacheType
{
    MM_CACHE_WRITEBACK = 0,
    MM_CACHE_UNCACHED,
    MM_CACHE_WRITE_COMBINE,
    MM_CACHE_WRITE_THROUGH,
} Mm_CacheType;

bool Arch_MmMapPage(uptr root, uptr virtualAddr, uptr physAddr, u32 prot, Mm_CacheType cacheType);
void Arch_MmUnmapPage(uptr root, uptr virtualAddr);

usize Arch_MmMapRegion(uptr root, uptr virtualAddr, uptr physAddr, usize size, u32 prot, Mm_CacheType cacheType);

uptr Arch_MmQueryMapping(uptr root, uptr virtualAddr);

void Arch_MmInvalidatePage(uptr virtualAddr);
void Arch_MmInvalidateRange(uptr virtualAddr, usize pageCount);

void Arch_MmFlushTlbGlobal(void);

uptr Arch_MmCreateUserPageTable(void);
void Arch_MmDestroyUserPageTable(uptr root);

void Arch_MmSwitchRoot(uptr root);
uptr Arch_MmGetCurrentRoot(void);

void Arch_MmInit();

#endif /* SUPERVISOR_ARCH_MMARCH_H */
