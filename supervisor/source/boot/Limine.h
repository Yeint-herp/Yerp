#ifndef SUPERVISOR_BOOT_LIMINE_H
#define SUPERVISOR_BOOT_LIMINE_H

#include <boot/Loader.h>
#include <limine.h>

extern volatile u64 Boot_LimineBaseRevision[];

bool Boot_Limine_Probe(void);

u64                 Boot_Limine_GetHhdmOffset(void);
uptr                Boot_Limine_GetRsdpPhys(void);
Boot_MemMap        *Boot_Limine_GetMemMap(void);
const Boot_SmpInfo *Boot_Limine_GetSmpInfo(void);
void                Boot_Limine_SetCpuExtra(u32 cpuIndex, uptr extra);
void                Boot_Limine_LaunchAps(Boot_ApEntry entry);

#endif /* SUPERVISOR_BOOT_LIMINE_H */
