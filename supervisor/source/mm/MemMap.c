#define DBG_MODULE "Mm"

#include <debug/DbgPrint.h>
#include <mm/MemMap.h>

const char *Mm_GetRegionTypeString(Mm_RegionType type)
{
    switch (type)
    {
        case kMemTypeUsable:
            return "USABLE";
        case kMemTypeReserved:
            return "RESERVED";
        case kMemTypeACPIReclaimable:
            return "ACPI_RECLAIMABLE";
        case kMemTypeACPINVS:
            return "ACPI_NVS";
        case kMemTypeBadMemory:
            return "BAD_MEMORY";
        case kMemTypeBootloaderReclaimable:
            return "BOOTLOADER_RECLAIMABLE";
        case kMemTypeSupervisorModules:
            return "SUPERVISOR_AND_MODULES";
        case kMemTypeFramebuffer:
            return "FRAMEBUFFER";
        case kMemTypeEarlyAllocated:
            return "EARLY_ALLOCATED";
        default:
            return "UNKNOWN";
    }
}

void Mm_DumpMemMap(const Mm_SupervisorMemMap *map)
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
