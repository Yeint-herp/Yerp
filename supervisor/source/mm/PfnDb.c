#define DBG_MODULE "MmPfnDb"

#include <arch/CoreLocal.h>
#include <arch/Irq.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/Early.h>
#include <mm/Magazine.h>
#include <mm/MemMap.h>
#include <mm/MmLayout.h>
#include <mm/PfnDb.h>

static Mm_Pfn *s_PfnDatabase        = nullptr;
static uptr    s_HighestPhysicalPfn = 0;
static uptr    s_TotalPhysicalPages = 0;

Mm_PfnList Mm_ZeroedPageListHead;
Mm_PfnList Mm_FreePageListHead;
Mm_PfnList Mm_StandbyPageListHead;
Mm_PfnList Mm_ModifiedPageListHead;
Mm_PfnList Mm_BadPageListHead;

static Mm_Pfn *s_PfnElement(uptr pfn)
{
    ASSERT(pfn <= s_HighestPhysicalPfn);
    return &s_PfnDatabase[pfn];
}

static uptr s_GetCeilingPfn(u32 flags)
{
    if (flags & MM_ALLOC_BELOW_16M)
        return 0x1000000ULL >> PAGE_SHIFT;

    if (flags & MM_ALLOC_BELOW_4G)
        return 0x100000000ULL >> PAGE_SHIFT;

    return s_HighestPhysicalPfn + 1;
}

static Mm_PfnList *s_GetListForLocation(Mm_PageLocation location)
{
    switch (location)
    {
        case ZeroedPageList:
            return &Mm_ZeroedPageListHead;
        case FreePageList:
            return &Mm_FreePageListHead;
        case StandbyPageList:
            return &Mm_StandbyPageListHead;
        case ModifiedPageList:
            return &Mm_ModifiedPageListHead;
        case ModifiedNoWriteList:
            return &Mm_ModifiedPageListHead;
        case BadPageList:
            return &Mm_BadPageListHead;
        default:
            return nullptr;
    }
}

static void s_InitializePfnList(Mm_PfnList *list, Mm_PageLocation name)
{
    list->Total    = 0;
    list->Flink    = MM_PFN_NULL;
    list->Blink    = MM_PFN_NULL;
    list->ListName = name;

    Core_SpinlockInit(&list->Lock);
}

static void s_UnlinkPageFromList(Mm_PfnList *list, uptr pfn)
{
    ASSERT(pfn != MM_PFN_NULL);
    Mm_Pfn *entry = s_PfnElement(pfn);

    Core_SpinlockAcquire(&list->Lock);

    uptr flink = entry->u1.Flink;
    uptr blink = entry->u2.Blink;

    if (blink != MM_PFN_NULL)
        s_PfnElement(blink)->u1.Flink = flink;
    else
        list->Flink = flink;

    if (flink != MM_PFN_NULL)
        s_PfnElement(flink)->u2.Blink = blink;
    else
        list->Blink = blink;

    list->Total--;

    entry->u1.Flink = MM_PFN_NULL;
    entry->u2.Blink = MM_PFN_NULL;

    Core_SpinlockRelease(&list->Lock);
}

static uptr s_RemoveConstrainedPage(Mm_PfnList *list, uptr ceilingPfn)
{
    Core_SpinlockAcquire(&list->Lock);

    uptr pfn = list->Flink;

    while (pfn != MM_PFN_NULL)
    {
        if (pfn < ceilingPfn)
        {
            Mm_Pfn *entry = s_PfnElement(pfn);
            uptr    flink = entry->u1.Flink;
            uptr    blink = entry->u2.Blink;

            if (blink != MM_PFN_NULL)
                s_PfnElement(blink)->u1.Flink = flink;
            else
                list->Flink = flink;

            if (flink != MM_PFN_NULL)
                s_PfnElement(flink)->u2.Blink = blink;
            else
                list->Blink = blink;

            list->Total--;

            entry->u1.Flink = MM_PFN_NULL;
            entry->u2.Blink = MM_PFN_NULL;

            Core_SpinlockRelease(&list->Lock);
            return pfn;
        }

        pfn = s_PfnElement(pfn)->u1.Flink;
    }

    Core_SpinlockRelease(&list->Lock);
    return MM_PFN_NULL;
}

static uptr s_ScanContiguous(uptr ceilingPfn, uptr count, bool acceptStandby)
{
    uptr runStart = 0;
    uptr runLen   = 0;

    for (uptr pfn = 0; pfn < ceilingPfn; pfn++)
    {
        Mm_Pfn         *entry = s_PfnElement(pfn);
        Mm_PageLocation loc   = (Mm_PageLocation)entry->e1.PageLocation;

        bool acceptable = (loc == FreePageList || loc == ZeroedPageList);
        if (acceptStandby && loc == StandbyPageList)
            acceptable = true;

        if (acceptable && entry->ReferenceCount == 0)
        {
            if (runLen == 0)
                runStart = pfn;

            runLen++;

            if (runLen == count)
                return runStart;
        }
        else
            runLen = 0;
    }

    return MM_PFN_NULL;
}

static void s_ClaimContiguousRun(uptr start, uptr count, u32 flags)
{
    for (uptr i = 0; i < count; i++)
    {
        uptr    pfn   = start + i;
        Mm_Pfn *entry = s_PfnElement(pfn);

        Core_SpinlockAcquire(&entry->Lock);

        Mm_PfnList *list = s_GetListForLocation((Mm_PageLocation)entry->e1.PageLocation);

        if (list)
        {
            Core_SpinlockRelease(&entry->Lock);
            s_UnlinkPageFromList(list, pfn);
            Core_SpinlockAcquire(&entry->Lock);
        }

        if (entry->e1.PageLocation == StandbyPageList)
            entry->OriginalPte = (Mm_PageTableEntry){};

        entry->e1.PageLocation = ActiveAndValid;
        entry->ReferenceCount  = 1;

        Core_SpinlockRelease(&entry->Lock);
    }

    if (flags & MM_ALLOC_ZEROED)
    {
        void *va = Mm_PhysToVirt(start << PAGE_SHIFT);
        Core_ZeroMemory(va, count * PAGE_SIZE);
    }
}

void Mm_InsertPageInList(Mm_PfnList *list, uptr pfn)
{
    ASSERT(pfn != MM_PFN_NULL);
    Mm_Pfn *entry = s_PfnElement(pfn);

    Core_SpinlockAcquire(&list->Lock);

    entry->u1.Flink = MM_PFN_NULL;
    entry->u2.Blink = list->Blink;

    if (list->Blink != MM_PFN_NULL)
        s_PfnElement(list->Blink)->u1.Flink = pfn;
    else
        list->Flink = pfn;

    list->Blink = pfn;
    list->Total++;

    Core_SpinlockRelease(&list->Lock);
}

uptr Mm_RemovePageFromList(Mm_PfnList *list)
{
    Core_SpinlockAcquire(&list->Lock);

    uptr pfn = list->Flink;
    if (pfn == MM_PFN_NULL)
    {
        Core_SpinlockRelease(&list->Lock);
        return MM_PFN_NULL;
    }

    Mm_Pfn *entry = s_PfnElement(pfn);

    list->Flink = entry->u1.Flink;

    if (list->Flink != MM_PFN_NULL)
        s_PfnElement(list->Flink)->u2.Blink = MM_PFN_NULL;
    else
        list->Blink = MM_PFN_NULL;

    list->Total--;

    entry->u1.Flink = MM_PFN_NULL;
    entry->u2.Blink = MM_PFN_NULL;

    Core_SpinlockRelease(&list->Lock);

    return pfn;
}

void Mm_TransitionPage(uptr pfn, Mm_PageLocation newState)
{
    ASSERT(pfn != MM_PFN_NULL && pfn <= s_HighestPhysicalPfn);

    Mm_Pfn *entry = s_PfnElement(pfn);

    Core_SpinlockAcquire(&entry->Lock);

    Mm_PageLocation oldState = (Mm_PageLocation)entry->e1.PageLocation;

    ASSERT((oldState == ZeroedPageList && newState == ActiveAndValid) ||
           (oldState == FreePageList && newState == ActiveAndValid) ||
           (oldState == StandbyPageList && newState == ActiveAndValid) ||
           (oldState == ActiveAndValid && newState == ModifiedPageList) ||
           (oldState == ActiveAndValid && newState == StandbyPageList) ||
           (oldState == ActiveAndValid && newState == FreePageList) ||
           (oldState == ModifiedPageList && newState == StandbyPageList) ||
           (oldState == FreePageList && newState == ZeroedPageList) ||
           (oldState == StandbyPageList && newState == FreePageList) || (newState == BadPageList));

    Mm_PfnList *oldList = s_GetListForLocation(oldState);
    if (oldList != nullptr)
        s_UnlinkPageFromList(oldList, pfn);

    entry->e1.PageLocation = newState;

    Mm_PfnList *newList = s_GetListForLocation(newState);
    if (newList != nullptr)
        Mm_InsertPageInList(newList, pfn);

    Core_SpinlockRelease(&entry->Lock);
}

static uptr s_MagazinePop(struct Mm_PfaMagazine *mag)
{
    if (mag->Count == 0)
        return MM_PFN_NULL;

    mag->Count--;
    return mag->Pages[mag->Count];
}

static bool s_MagazinePush(struct Mm_PfaMagazine *mag, uptr pfn)
{
    if (mag->Count >= MM_PFA_MAGAZINE_SIZE)
        return false;

    mag->Pages[mag->Count] = pfn;
    mag->Count++;
    return true;
}

static void s_MagazineRefill(struct Mm_PfaMagazine *mag, Mm_PfnList *list)
{
    u16 target = MM_PFA_MAGAZINE_SIZE / 2;

    while (mag->Count < target)
    {
        uptr pfn = Mm_RemovePageFromList(list);
        if (pfn == MM_PFN_NULL)
            break;

        Mm_Pfn *entry = s_PfnElement(pfn);

        Core_SpinlockAcquire(&entry->Lock);
        entry->e1.PageLocation   = ActiveAndValid;
        entry->e1.MagazineCached = 1;
        entry->ReferenceCount    = 1;
        Core_SpinlockRelease(&entry->Lock);

        mag->Pages[mag->Count++] = pfn;
    }
}

static void s_MagazineDrain(struct Mm_PfaMagazine *mag, Mm_PfnList *list)
{
    u16 target = mag->Count / 2;

    while (mag->Count > target)
    {
        mag->Count--;
        uptr pfn = mag->Pages[mag->Count];

        Mm_Pfn *entry = s_PfnElement(pfn);

        Core_SpinlockAcquire(&entry->Lock);
        entry->e1.PageLocation   = list->ListName;
        entry->e1.MagazineCached = 0;
        entry->ReferenceCount    = 0;
        entry->PteFrame          = 0;
        entry->OriginalPte       = (Mm_PageTableEntry){};
        entry->u2.ShareCount     = 0;
        Core_SpinlockRelease(&entry->Lock);

        Mm_InsertPageInList(list, pfn);
    }
}

static uptr s_AllocateConstrained(u32 flags)
{
    bool needZero = (flags & MM_ALLOC_ZEROED) != 0;
    uptr ceiling  = s_GetCeilingPfn(flags);

    Mm_PfnList *primary, *secondary;

    if (needZero)
    {
        primary   = &Mm_ZeroedPageListHead;
        secondary = &Mm_FreePageListHead;
    }
    else
    {
        primary   = &Mm_FreePageListHead;
        secondary = &Mm_ZeroedPageListHead;
    }

    uptr pfn = s_RemoveConstrainedPage(primary, ceiling);
    if (pfn == MM_PFN_NULL)
        pfn = s_RemoveConstrainedPage(secondary, ceiling);
    if (pfn == MM_PFN_NULL)
        pfn = s_RemoveConstrainedPage(&Mm_StandbyPageListHead, ceiling);

    if (pfn == MM_PFN_NULL)
        return MM_PFN_NULL;

    Mm_Pfn *entry = s_PfnElement(pfn);

    Core_SpinlockAcquire(&entry->Lock);

    bool wasFree    = (entry->e1.PageLocation == FreePageList);
    bool wasStandby = (entry->e1.PageLocation == StandbyPageList);

    if (wasStandby)
        entry->OriginalPte = (Mm_PageTableEntry){};

    entry->e1.PageLocation = ActiveAndValid;
    entry->ReferenceCount  = 1;

    Core_SpinlockRelease(&entry->Lock);

    if (needZero && (wasFree || wasStandby))
    {
        void *va = Mm_PhysToVirt(pfn << PAGE_SHIFT);
        Core_ZeroMemory(va, PAGE_SIZE);
    }

    return pfn;
}

static uptr s_AllocateContiguousPages(u32 flags, uptr count)
{
    uptr ceiling = s_GetCeilingPfn(flags);

    uptr pfn = s_ScanContiguous(ceiling, count, false);
    if (pfn != MM_PFN_NULL)
    {
        s_ClaimContiguousRun(pfn, count, flags);
        return pfn;
    }

    pfn = s_ScanContiguous(ceiling, count, true);
    if (pfn != MM_PFN_NULL)
    {
        s_ClaimContiguousRun(pfn, count, flags);
        return pfn;
    }

    return MM_PFN_NULL;
}

uptr Mm_AllocatePages(u32 flags, uptr count)
{
    ASSERT(count >= 1);

    if ((flags & MM_ALLOC_CONTIGUOUS) || count > 1)
    {
        uptr pfn = s_AllocateContiguousPages(flags, count);
        if (pfn == MM_PFN_NULL)
            Log(WARN, "contiguous allocation failed (count = %llu, flags = %#x)", count, flags);

        return pfn;
    }

    if (flags & (MM_ALLOC_BELOW_4G | MM_ALLOC_BELOW_16M))
    {
        uptr pfn = s_AllocateConstrained(flags);
        if (pfn == MM_PFN_NULL)
            Log(WARN, "constrained allocation failed (flags = %#x)", flags);

        return pfn;
    }

    bool needZero = (flags & MM_ALLOC_ZEROED) != 0;

    Arch_IrqFlags     irq  = Arch_IrqSave();
    struct Core_SPCB *spcb = Arch_GetCurrentSpcb();

    struct Mm_PfaMagazine *preferred, *fallback;
    if (needZero)
    {
        preferred = &spcb->ZeroPages;
        fallback  = &spcb->FreePages;
    }
    else
    {
        preferred = &spcb->FreePages;
        fallback  = &spcb->ZeroPages;
    }

    uptr pfn = s_MagazinePop(preferred);
    if (pfn == MM_PFN_NULL)
        pfn = s_MagazinePop(fallback);

    if (pfn != MM_PFN_NULL)
    {
        Mm_Pfn *entry = s_PfnElement(pfn);

        Core_SpinlockAcquire(&entry->Lock);
        entry->e1.MagazineCached = 0;
        Core_SpinlockRelease(&entry->Lock);

        Arch_IrqRestore(irq);

        if (needZero && preferred != &spcb->ZeroPages)
        {
            void *va = Mm_PhysToVirt(pfn << PAGE_SHIFT);
            Core_ZeroMemory(va, PAGE_SIZE);
        }

        return pfn;
    }

    if (needZero)
    {
        s_MagazineRefill(&spcb->ZeroPages, &Mm_ZeroedPageListHead);
        if (spcb->ZeroPages.Count == 0)
            s_MagazineRefill(&spcb->ZeroPages, &Mm_FreePageListHead);

        pfn = s_MagazinePop(&spcb->ZeroPages);
    }
    else
    {
        s_MagazineRefill(&spcb->FreePages, &Mm_FreePageListHead);
        if (spcb->FreePages.Count == 0)
            s_MagazineRefill(&spcb->FreePages, &Mm_ZeroedPageListHead);

        pfn = s_MagazinePop(&spcb->FreePages);
    }

    if (pfn != MM_PFN_NULL)
    {
        Mm_Pfn *entry = s_PfnElement(pfn);

        Core_SpinlockAcquire(&entry->Lock);
        entry->e1.MagazineCached = 0;
        Core_SpinlockRelease(&entry->Lock);

        Arch_IrqRestore(irq);

        if (needZero)
        {
            void *va = Mm_PhysToVirt(pfn << PAGE_SHIFT);
            Core_ZeroMemory(va, PAGE_SIZE);
        }

        return pfn;
    }

    Arch_IrqRestore(irq);

    pfn = Mm_RemovePageFromList(&Mm_StandbyPageListHead);
    if (pfn != MM_PFN_NULL)
    {
        Mm_Pfn *entry = s_PfnElement(pfn);

        Core_SpinlockAcquire(&entry->Lock);
        entry->OriginalPte     = (Mm_PageTableEntry){};
        entry->e1.PageLocation = ActiveAndValid;
        entry->ReferenceCount  = 1;
        Core_SpinlockRelease(&entry->Lock);

        if (needZero)
        {
            void *va = Mm_PhysToVirt(pfn << PAGE_SHIFT);
            Core_ZeroMemory(va, PAGE_SIZE);
        }

        return pfn;
    }

    Log(WARN, "page allocation failed (flags = %#x)", flags);
    return MM_PFN_NULL;
}

void Mm_FreePages(uptr pfn, uptr count)
{
    for (uptr i = 0; i < count; i++)
    {
        uptr p = pfn + i;
        ASSERT(p != MM_PFN_NULL && p <= s_HighestPhysicalPfn);

        Mm_Pfn *entry = s_PfnElement(p);

        Core_SpinlockAcquire(&entry->Lock);

        ASSERT(entry->e1.PageLocation == ActiveAndValid);
        ASSERT(entry->e1.MagazineCached == 0);
        ASSERT(entry->ReferenceCount >= 1);

        entry->ReferenceCount--;

        if (entry->ReferenceCount > 0)
        {
            Core_SpinlockRelease(&entry->Lock);
            continue;
        }

        entry->e1.MagazineCached = 1;
        Core_SpinlockRelease(&entry->Lock);

        Arch_IrqFlags     irq  = Arch_IrqSave();
        struct Core_SPCB *spcb = Arch_GetCurrentSpcb();

        if (s_MagazinePush(&spcb->FreePages, p))
        {
            Arch_IrqRestore(irq);
            continue;
        }

        s_MagazineDrain(&spcb->FreePages, &Mm_FreePageListHead);

        bool pushed = s_MagazinePush(&spcb->FreePages, p);
        ASSERT(pushed);

        Arch_IrqRestore(irq);
    }
}

Mm_Pfn *Mm_GetPfnEntry(uptr pfn)
{
    if (pfn > s_HighestPhysicalPfn)
        return nullptr;

    return s_PfnElement(pfn);
}

uptr Mm_GetTotalPhysicalPages(void)
{
    return s_TotalPhysicalPages;
}

uptr Mm_GetFreePageCount(void)
{
    return Mm_ZeroedPageListHead.Total + Mm_FreePageListHead.Total;
}

void Mm_PfnDbInit(void)
{
    const Mm_KernelMemMap *memMap = Mm_GetKernelMemMap();
    const Mm_VaLayout     *layout = Mm_GetVaLayout();

    u64 highestAddress = 0;
    for (usize i = 0; i < memMap->Count; i++)
    {
        u64 regionEnd = memMap->Regions[i].Base + memMap->Regions[i].Length;
        if (regionEnd > highestAddress)
            highestAddress = regionEnd;
    }

    s_HighestPhysicalPfn = (highestAddress >> PAGE_SHIFT) - 1;
    s_PfnDatabase        = (Mm_Pfn *)layout->PfnDbBase;

    const usize pfnDbMaxSize = (s_HighestPhysicalPfn + 1) * sizeof(Mm_Pfn);
    ASSERT(pfnDbMaxSize <= layout->PfnDbSize);

    Log(INFO, "PfnDb at VA %p, highest PFN #%llu", s_PfnDatabase, s_HighestPhysicalPfn);

    usize totalBackingPages = 0;
    uptr  lastMappedPage    = MM_PFN_NULL;
    for (usize i = 0; i < memMap->Count; i++)
    {
        const Mm_MemRegion *region = &memMap->Regions[i];

        switch (region->Type)
        {
            case MEM_TYPE_USABLE:
            case MEM_TYPE_SUPERVISOR_MODULES:
            case MEM_TYPE_EARLY_ALLOCATED:
            case MEM_TYPE_BOOTLOADER_RECLAIMABLE:
            case MEM_TYPE_ACPI_RECLAIMABLE:
            case MEM_TYPE_BAD_MEMORY:
            case MEM_TYPE_FRAMEBUFFER:
                break;
            default:
                continue;
        }

        const uptr startPfn = region->Base >> PAGE_SHIFT;
        const uptr endPfn   = (region->Base + region->Length) >> PAGE_SHIFT;

        const uptr dbStartByte = startPfn * sizeof(Mm_Pfn);
        const uptr dbEndByte   = endPfn * sizeof(Mm_Pfn);

        uptr       mapStart = dbStartByte & ~(PAGE_SIZE - 1);
        const uptr mapEnd   = (dbEndByte + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        if (lastMappedPage != MM_PFN_NULL && mapStart <= lastMappedPage)
            mapStart = lastMappedPage + PAGE_SIZE;

        if (mapStart < mapEnd)
        {
            totalBackingPages += (mapEnd - mapStart) / PAGE_SIZE;
            lastMappedPage = mapEnd - PAGE_SIZE;
        }
    }

    const usize backingSize = totalBackingPages * PAGE_SIZE;
    Log(INFO, "allocating %llu pages (%llZ)", totalBackingPages, backingSize);

    void *backingBlock = Mm_EarlyAllocate(backingSize, PAGE_SIZE);
    if (!backingBlock)
        Panic("failed to allocate PfnDb backing");

    uptr backingPhysCursor = Mm_VirtToPhys(backingBlock);
    uptr lastMappedOffset  = MM_PFN_NULL;

    for (usize i = 0; i < memMap->Count; i++)
    {
        const Mm_MemRegion *region = &memMap->Regions[i];

        switch (region->Type)
        {
            case MEM_TYPE_USABLE:
            case MEM_TYPE_SUPERVISOR_MODULES:
            case MEM_TYPE_EARLY_ALLOCATED:
            case MEM_TYPE_BOOTLOADER_RECLAIMABLE:
            case MEM_TYPE_ACPI_RECLAIMABLE:
            case MEM_TYPE_BAD_MEMORY:
            case MEM_TYPE_FRAMEBUFFER:
                break;
            default:
                continue;
        }

        const uptr startPfn    = region->Base >> PAGE_SHIFT;
        const uptr endPfn      = (region->Base + region->Length) >> PAGE_SHIFT;
        const uptr dbStartByte = startPfn * sizeof(Mm_Pfn);
        const uptr dbEndByte   = endPfn * sizeof(Mm_Pfn);

        uptr       mapStart = dbStartByte & ~(PAGE_SIZE - 1);
        const uptr mapEnd   = (dbEndByte + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        if (lastMappedOffset != MM_PFN_NULL && mapStart <= lastMappedOffset)
            mapStart = lastMappedOffset + PAGE_SIZE;

        for (uptr offset = mapStart; offset < mapEnd; offset += PAGE_SIZE)
        {
            uptr va = layout->PfnDbBase + offset;

            bool ok = Mm_EarlyMapPage(va, backingPhysCursor, MM_MAP_WRITE | MM_MAP_NOEXEC | MM_MAP_GLOBAL);
            if (!ok)
                Panic("failed to map PfnDb page");

            backingPhysCursor += PAGE_SIZE;
        }

        if (mapEnd > mapStart)
            lastMappedOffset = mapEnd - PAGE_SIZE;
    }

    s_InitializePfnList(&Mm_ZeroedPageListHead, ZeroedPageList);
    s_InitializePfnList(&Mm_FreePageListHead, FreePageList);
    s_InitializePfnList(&Mm_StandbyPageListHead, StandbyPageList);
    s_InitializePfnList(&Mm_ModifiedPageListHead, ModifiedPageList);
    s_InitializePfnList(&Mm_BadPageListHead, BadPageList);

    for (usize i = 0; i < memMap->Count; i++)
    {
        const Mm_MemRegion *region = &memMap->Regions[i];

        switch (region->Type)
        {
            case MEM_TYPE_USABLE:
            case MEM_TYPE_SUPERVISOR_MODULES:
            case MEM_TYPE_EARLY_ALLOCATED:
            case MEM_TYPE_BOOTLOADER_RECLAIMABLE:
            case MEM_TYPE_ACPI_RECLAIMABLE:
            case MEM_TYPE_BAD_MEMORY:
            case MEM_TYPE_FRAMEBUFFER:
                break;
            default:
                continue;
        }

        uptr startPfn = region->Base >> PAGE_SHIFT;
        uptr endPfn   = (region->Base + region->Length) >> PAGE_SHIFT;

        for (uptr pfn = startPfn; pfn < endPfn; pfn++)
        {
            Mm_Pfn *entry = s_PfnElement(pfn);
            Core_SpinlockInit(&entry->Lock);

            switch (region->Type)
            {
                case MEM_TYPE_USABLE:
                    entry->e1.PageLocation = FreePageList;
                    entry->ReferenceCount  = 0;
                    Mm_InsertPageInList(&Mm_FreePageListHead, pfn);
                    s_TotalPhysicalPages++;
                    break;

                case MEM_TYPE_BAD_MEMORY:
                    entry->e1.PageLocation = BadPageList;
                    entry->ReferenceCount  = 0;
                    Mm_InsertPageInList(&Mm_BadPageListHead, pfn);
                    break;

                case MEM_TYPE_SUPERVISOR_MODULES:
                case MEM_TYPE_EARLY_ALLOCATED:
                case MEM_TYPE_FRAMEBUFFER:
                case MEM_TYPE_BOOTLOADER_RECLAIMABLE:
                case MEM_TYPE_ACPI_RECLAIMABLE:
                    entry->e1.PageLocation = ActiveAndValid;
                    entry->ReferenceCount  = 1;
                    break;

                case MEM_TYPE_RESERVED:
                case MEM_TYPE_ACPI_NVS:
                default:
                    break;
            }
        }
    }

    Log(INFO, "  free pages:           %llu", Mm_FreePageListHead.Total);
    Log(INFO, "  bad pages:            %llu", Mm_BadPageListHead.Total);
}
