#ifndef SUPERVISOR_BOOT_LOADER_H
#define SUPERVISOR_BOOT_LOADER_H

#include <arch/Spcr.h>
#include <mm/MemMap.h>

typedef struct
{
    u64 Base;
    u64 Length;

    Mm_RegionType Type;
} Boot_MemEntry;

typedef struct
{
    Boot_MemEntry *Entries;
    usize          Count;
} Boot_MemMap;

typedef struct
{
    u32 Count;

    ArchId_t  BspArchId;
    ArchId_t *ArchIds;
} Boot_SmpInfo;

bool Boot_Init(void);

u64                 Boot_GetHhdmOffset(void);
uptr                Boot_GetRsdpPhys(void);
Boot_MemMap        *Boot_GetMemMap(void);
const Boot_SmpInfo *Boot_GetSmpInfo(void);

void Boot_SetCpuExtra(u32 cpuIndex, uptr extra);

typedef void (*Boot_ApEntry)(uptr extra);
void Boot_LaunchAps(Boot_ApEntry entry);

#endif /* SUPERVISOR_BOOT_LOADER_H */
