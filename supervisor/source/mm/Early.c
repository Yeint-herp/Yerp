#define DBG_MODULE "MmEarly"

#include <core/Memory.h>
#include <core/Spinlock.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Init.h>
#include <mm/Early.h>
#include <mm/MemMap.h>
#include <mm/PfnDb.h>

static Mm_SupervisorMemMap s_MemMap    = {};
static Core_Spinlock       s_AllocLock = {};

static bool  s_PfnReady   = false;
static void *s_BumpPage   = nullptr;
static usize s_BumpOffset = PAGE_SIZE;

const Mm_SupervisorMemMap *Mm_GetSupervisorMemMap(void)
{
    return &s_MemMap;
}

static u64 s_HhdmOffset = 0;

u64 Mm_GetHhdmBase(void)
{
    return s_HhdmOffset;
}

void *Mm_PhysToVirt(uptr physAddr)
{
    return (void *)(physAddr + Mm_GetHhdmBase());
}

uptr Mm_VirtToPhys(void *virtAddr)
{
    return (uptr)virtAddr - Mm_GetHhdmBase();
}

void Mm_SetPfnReady(void)
{
    s_PfnReady = true;
    Log(INFO, "permanent allocator switched to PFN-backed mode");
}

bool Mm_IsPfnReady(void)
{
    return s_PfnReady;
}

static bool s_InsertRegion(usize pos, u64 base, u64 length, Mm_RegionType type)
{
    if (s_MemMap.Count >= s_MemMap.Capacity)
        return false;

    Mm_MemRegion *dst     = &s_MemMap.Regions[pos + 1];
    Mm_MemRegion *src     = &s_MemMap.Regions[pos];
    usize         moveLen = (s_MemMap.Count - pos) * sizeof(Mm_MemRegion);

    Core_MoveMemory(dst, src, moveLen);

    s_MemMap.Regions[pos].Base   = base;
    s_MemMap.Regions[pos].Length = length;
    s_MemMap.Regions[pos].Type   = type;
    s_MemMap.Count++;
    return true;
}

void Mm_EarlyInit(Boot_MemMap *bootMap, u64 hhdmOffset)
{
    s_HhdmOffset = hhdmOffset;

    usize entryCount = bootMap->Count;
    usize capacity   = entryCount + 32;
    usize arraySize  = capacity * sizeof(Mm_MemRegion);

    u64  arrayPhys  = 0;
    bool foundSpace = false;

    for (usize i = entryCount; i > 0; i--)
    {
        usize          idx   = i - 1;
        Boot_MemEntry *entry = &bootMap->Entries[idx];

        if (entry->Type == kMemTypeUsable && entry->Length >= arraySize)
        {
            arrayPhys  = entry->Base + entry->Length - arraySize;
            foundSpace = true;
            entry->Length -= arraySize;
            break;
        }
    }

    if (!foundSpace)
        Panic("failed to carve space for the early memory map");

    s_MemMap.Regions  = (Mm_MemRegion *)(arrayPhys + s_HhdmOffset);
    s_MemMap.Capacity = capacity;
    s_MemMap.Count    = 0;

    Core_ZeroMemory(s_MemMap.Regions, arraySize);

    s_MemMap.Regions[0].Base   = arrayPhys;
    s_MemMap.Regions[0].Length = arraySize;
    s_MemMap.Regions[0].Type   = kMemTypeEarlyAllocated;
    s_MemMap.Count++;

    for (usize i = 0; i < entryCount; i++)
    {
        Boot_MemEntry *entry = &bootMap->Entries[i];
        if (entry->Length == 0)
            continue;

        s_MemMap.Regions[s_MemMap.Count].Base   = entry->Base;
        s_MemMap.Regions[s_MemMap.Count].Length = entry->Length;
        s_MemMap.Regions[s_MemMap.Count].Type   = entry->Type;
        s_MemMap.Count++;
    }

    for (usize i = 1; i < s_MemMap.Count; i++)
    {
        Mm_MemRegion key = s_MemMap.Regions[i];
        isize        j   = i - 1;

        while (j >= 0 && s_MemMap.Regions[j].Base > key.Base)
        {
            s_MemMap.Regions[j + 1] = s_MemMap.Regions[j];
            j--;
        }
        s_MemMap.Regions[j + 1] = key;
    }

    Log(INFO, "early memory map initialized. %zu entries translated", s_MemMap.Count);
}

static void *s_CarveFromMemMap(usize size, usize alignment)
{
    u64 allocPhys = 0;
    for (usize i = 0; i < s_MemMap.Count; i++)
    {
        Mm_MemRegion *reg = &s_MemMap.Regions[i];
        if (reg->Type != kMemTypeUsable)
            continue;

        if (reg->Length < size)
            continue;

        const u64 regTop      = reg->Base + reg->Length;
        const u64 alignedBase = AlignDown(regTop - size, alignment);

        if (alignedBase < reg->Base)
            continue;

        if (alignedBase < 0x1000)
            continue;

        allocPhys = alignedBase;

        const u64 belowLen = alignedBase - reg->Base;
        const u64 aboveLen = regTop - (alignedBase + size);

        if (belowLen == 0 && aboveLen == 0)
            reg->Type = kMemTypeEarlyAllocated;
        else if (belowLen > 0 && aboveLen == 0)
        {
            reg->Length = belowLen;

            if (!s_InsertRegion(i + 1, allocPhys, size, kMemTypeEarlyAllocated))
                Log(WARN, "early map full, allocation %#llx not tracked", allocPhys);
        }
        else if (belowLen == 0 && aboveLen > 0)
        {
            reg->Base   = allocPhys;
            reg->Length = size;
            reg->Type   = kMemTypeEarlyAllocated;

            if (!s_InsertRegion(i + 1, allocPhys + size, aboveLen, kMemTypeUsable))
                Log(WARN, "early map full, %llu bytes above %#llx lost", aboveLen, allocPhys);
        }
        else
        {
            reg->Length = belowLen;

            if (!s_InsertRegion(i + 1, allocPhys, size, kMemTypeEarlyAllocated))
            {
                Log(WARN, "early map full, allocation %#llx not tracked", allocPhys);
                break;
            }

            if (!s_InsertRegion(i + 2, allocPhys + size, aboveLen, kMemTypeUsable))
                Log(WARN, "early map full, %llu bytes above %#llx lost", aboveLen, allocPhys);
        }

        break;
    }

    if (!allocPhys)
        return nullptr;

    void *ptr = Mm_PhysToVirt(allocPhys);
    Core_ZeroMemory(ptr, size);
    return ptr;
}

static void *s_AllocFromPfn(usize size, usize alignment)
{
    if (size >= PAGE_SIZE && alignment <= PAGE_SIZE)
    {
        const uptr count = AlignUp(size, PAGE_SIZE) >> PAGE_SHIFT;
        u32        flags = MM_ALLOC_ZEROED;

        if (count > 1)
            flags |= MM_ALLOC_CONTIGUOUS;

        const uptr pfn = Mm_AllocatePages(flags, count);
        if (pfn == MM_PFN_NULL)
            return nullptr;

        return Mm_PhysToVirt(pfn << PAGE_SHIFT);
    }

    usize alignedOff = AlignUp(s_BumpOffset, alignment);
    if (alignedOff + size > PAGE_SIZE)
    {
        uptr pfn = Mm_AllocatePages(MM_ALLOC_ZEROED, 1);
        if (pfn == MM_PFN_NULL)
            return nullptr;

        s_BumpPage   = Mm_PhysToVirt(pfn << PAGE_SHIFT);
        s_BumpOffset = 0;
        alignedOff   = 0;
    }

    void *ptr    = s_BumpPage + alignedOff;
    s_BumpOffset = alignedOff + size;

    return ptr;
}

void *Mm_PermanentAllocate(usize size, usize alignment)
{
    if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
        return nullptr;

    Arch_IrqFlags flags = Core_SpinlockAcquireIrq(&s_AllocLock);

    void *ptr;

    if (s_PfnReady)
        ptr = s_AllocFromPfn(size, alignment);
    else
        ptr = s_CarveFromMemMap(size, alignment);

    Core_SpinlockReleaseIrq(&s_AllocLock, flags);

    if (!ptr)
        Log(FATAL, "permanent allocation failed for size %zu", size);

    return ptr;
}
