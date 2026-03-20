#define DBG_MODULE "Mm"

#include <debug/DbgPrint.h>
#include <mm/MemMap.h>

const char *Mm_GetRegionTypeString(Mm_RegionType type)
{
    switch (type)
    {
        case MEM_TYPE_USABLE:
            return "USABLE";
        case MEM_TYPE_RESERVED:
            return "RESERVED";
        case MEM_TYPE_ACPI_RECLAIMABLE:
            return "ACPI_RECLAIMABLE";
        case MEM_TYPE_ACPI_NVS:
            return "ACPI_NVS";
        case MEM_TYPE_BAD_MEMORY:
            return "BAD_MEMORY";
        case MEM_TYPE_BOOTLOADER_RECLAIMABLE:
            return "BOOTLOADER_RECLAIMABLE";
        case MEM_TYPE_KERNEL_AND_MODULES:
            return "KERNEL_AND_MODULES";
        case MEM_TYPE_FRAMEBUFFER:
            return "FRAMEBUFFER";
        case MEM_TYPE_EARLY_ALLOCATED:
            return "EARLY_ALLOCATED";
        default:
            return "UNKNOWN";
    }
}

void Mm_DumpMemMap(const Mm_KernelMemMap *map)
{
    if (!map || !map->Regions)
    {
        Log(WARN, "memory map is null");
        return;
    }

    for (usize i = 0; i < map->Count; i++)
    {
        const Mm_MemRegion *reg = &map->Regions[i];

        u64 top = reg->Base + reg->Length;

        Log(TRACE, "[%02zu] %#018llx - %#018llx | Size: %-8zZ | %-25s", i, reg->Base, top, reg->Length,
            Mm_GetRegionTypeString(reg->Type));
    }
}
