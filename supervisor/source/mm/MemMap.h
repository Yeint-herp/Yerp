#ifndef SUPERVISOR_MM_MEMMAP_H
#define SUPERVISOR_MM_MEMMAP_H

typedef enum
{
    kMemTypeUsable,
    kMemTypeReserved,
    kMemTypeACPIReclaimable,
    kMemTypeACPINVS,
    kMemTypeBadMemory,
    kMemTypeBootloaderReclaimable,
    kMemTypeSupervisorModules,
    kMemTypeFramebuffer,
    kMemTypeEarlyAllocated
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
} Mm_SupervisorMemMap;

const char *Mm_GetRegionTypeString(Mm_RegionType type);
void        Mm_DumpMemMap(const Mm_SupervisorMemMap *map);

#endif /* SUPERVISOR_MM_MEMMAP_H */
