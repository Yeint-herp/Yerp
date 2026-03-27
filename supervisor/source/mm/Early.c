#define DBG_MODULE "MmEarly"

#include <core/Memory.h>
#include <core/Spinlock.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Init.h>
#include <limine.h>
#include <mm/Early.h>
#include <mm/MemMap.h>
#include <mm/PfnDb.h>

static Mm_SupervisorMemMap s_MemMap         = {};
static Core_Spinlock   s_EarlyAllocLock = {};

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

static Mm_RegionType s_TranslateLimineType(u64 limineType)
{
    switch (limineType)
    {
        case LIMINE_MEMMAP_USABLE:
            return MEM_TYPE_USABLE;
        case LIMINE_MEMMAP_RESERVED:
            return MEM_TYPE_RESERVED;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return MEM_TYPE_ACPI_RECLAIMABLE;
        case LIMINE_MEMMAP_ACPI_NVS:
            return MEM_TYPE_ACPI_NVS;
        case LIMINE_MEMMAP_BAD_MEMORY:
            return MEM_TYPE_BAD_MEMORY;
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return MEM_TYPE_BOOTLOADER_RECLAIMABLE;
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            return MEM_TYPE_SUPERVISOR_MODULES;
        case LIMINE_MEMMAP_FRAMEBUFFER:
            return MEM_TYPE_FRAMEBUFFER;
        default:
            return MEM_TYPE_RESERVED;
    }
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

void Mm_EarlyInit(struct limine_memmap_response *mmResponse, u64 hhdmOffset)
{
    s_HhdmOffset = hhdmOffset;

    usize capacity  = mmResponse->entry_count + 32;
    usize arraySize = capacity * sizeof(Mm_MemRegion);

    u64  arrayPhys  = 0;
    bool foundSpace = false;

    for (usize i = mmResponse->entry_count; i > 0; i--)
    {
        usize                       idx   = i - 1;
        struct limine_memmap_entry *entry = mmResponse->entries[idx];

        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= arraySize)
        {
            arrayPhys  = entry->base + entry->length - arraySize;
            foundSpace = true;

            entry->length -= arraySize;
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
    s_MemMap.Regions[0].Type   = MEM_TYPE_EARLY_ALLOCATED;
    s_MemMap.Count++;

    for (usize i = 0; i < mmResponse->entry_count; i++)
    {
        struct limine_memmap_entry *entry = mmResponse->entries[i];
        if (entry->length == 0)
            continue;

        s_MemMap.Regions[s_MemMap.Count].Base   = entry->base;
        s_MemMap.Regions[s_MemMap.Count].Length = entry->length;
        s_MemMap.Regions[s_MemMap.Count].Type   = s_TranslateLimineType(entry->type);
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

void *Mm_EarlyAllocate(usize size, usize alignment)
{
    if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
        return nullptr;

    Arch_IrqFlags flags = Core_SpinlockAcquireIrq(&s_EarlyAllocLock);

    u64 allocPhys = 0;
    for (usize i = 0; i < s_MemMap.Count; i++)
    {
        Mm_MemRegion *reg = &s_MemMap.Regions[i];
        if (reg->Type != MEM_TYPE_USABLE)
            continue;

        if (reg->Length < size)
            continue;

        u64 regTop      = reg->Base + reg->Length;
        u64 alignedBase = AlignDown(regTop - size, alignment);

        if (alignedBase < reg->Base)
            continue;

        if (alignedBase < 0x1000)
            continue;

        allocPhys = alignedBase;

        u64 belowLen = alignedBase - reg->Base;
        u64 aboveLen = regTop - (alignedBase + size);

        if (belowLen == 0 && aboveLen == 0)
            reg->Type = MEM_TYPE_EARLY_ALLOCATED;
        else if (belowLen > 0 && aboveLen == 0)
        {
            reg->Length = belowLen;

            if (!s_InsertRegion(i + 1, allocPhys, size, MEM_TYPE_EARLY_ALLOCATED))
                Log(WARN, "early map full, allocation %#llx not tracked", allocPhys);
        }
        else if (belowLen == 0 && aboveLen > 0)
        {
            reg->Base   = allocPhys;
            reg->Length = size;
            reg->Type   = MEM_TYPE_EARLY_ALLOCATED;

            if (!s_InsertRegion(i + 1, allocPhys + size, aboveLen, MEM_TYPE_USABLE))
                Log(WARN, "early map full, %llu bytes above %#llx lost", aboveLen, allocPhys);
        }
        else
        {
            reg->Length = belowLen;

            if (!s_InsertRegion(i + 1, allocPhys, size, MEM_TYPE_EARLY_ALLOCATED))
            {
                Log(WARN, "early map full, allocation %#llx not tracked", allocPhys);
                break;
            }

            if (!s_InsertRegion(i + 2, allocPhys + size, aboveLen, MEM_TYPE_USABLE))
                Log(WARN, "early map full, %llu bytes above %#llx lost", aboveLen, allocPhys);
        }

        break;
    }

    Core_SpinlockReleaseIrq(&s_EarlyAllocLock, flags);

    if (!allocPhys)
    {
        Log(FATAL, "early allocation failed for size %zu", size);
        return nullptr;
    }

    void *virt_ptr = Mm_PhysToVirt(allocPhys);
    Core_ZeroMemory(virt_ptr, size);

    return virt_ptr;
}
