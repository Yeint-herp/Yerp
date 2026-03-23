#ifndef SUPERVISOR_MM_EARLY_H
#define SUPERVISOR_MM_EARLY_H

#include <mm/MemMap.h>

struct limine_memmap_response;

void  Mm_EarlyInit(struct limine_memmap_response *mmResponse, u64 hhdmOffset);
void *Mm_EarlyAllocate(usize size, usize alignment);

const Mm_KernelMemMap *Mm_GetKernelMemMap(void);
u64                    Mm_GetHhdmBase(void);

void *Mm_PhysToVirt(uptr physAddr);
uptr  Mm_VirtToPhys(void *virtAddr);

bool Mm_EarlyMapPage(uptr virtualAddr, uptr physAddr, u32 flags);

#define MM_MAP_WRITE   (1U << 0)
#define MM_MAP_NOEXEC  (1U << 1)
#define MM_MAP_NOCACHE (1U << 2)
#define MM_MAP_GLOBAL  (1U << 3)

#endif /* SUPERVISOR_MM_EARLY_H */
