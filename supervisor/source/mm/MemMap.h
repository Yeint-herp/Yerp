#ifndef SUPERVISOR_MM_MEMMAP_H
#define SUPERVISOR_MM_MEMMAP_H

typedef enum
{
    MEM_TYPE_USABLE,
    MEM_TYPE_RESERVED,
    MEM_TYPE_ACPI_RECLAIMABLE,
    MEM_TYPE_ACPI_NVS,
    MEM_TYPE_BAD_MEMORY,
    MEM_TYPE_BOOTLOADER_RECLAIMABLE,
    MEM_TYPE_SUPERVISOR_MODULES,
    MEM_TYPE_FRAMEBUFFER,
    MEM_TYPE_EARLY_ALLOCATED
} Mm_RegionType;

typedef struct
{
    u64 Base;
    u64 Length;

    Mm_RegionType Type;
} Mm_MemRegion;

typedef struct
{
    Mm_MemRegion *Regions;

    usize Count;
    usize Capacity;
} Mm_KernelMemMap;

const char *Mm_GetRegionTypeString(Mm_RegionType type);
void        Mm_DumpMemMap(const Mm_KernelMemMap *map);

#endif /* SUPERVISOR_MM_MEMMAP_H */
