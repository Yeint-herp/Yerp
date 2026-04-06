#ifndef SUPERVISOR_MM_EARLY_H
#define SUPERVISOR_MM_EARLY_H

#include <boot/Loader.h>
#include <mm/MemMap.h>

void  Mm_EarlyInit(Boot_MemMap *bootMap, u64 hhdmOffset);
void *Mm_PermanentAllocate(usize size, usize alignment);

void Mm_SetPfnReady(void);
bool Mm_IsPfnReady(void);

const Mm_SupervisorMemMap *Mm_GetSupervisorMemMap(void);
u64                        Mm_GetHhdmBase(void);

void *Mm_PhysToVirt(uptr physAddr);
uptr  Mm_VirtToPhys(void *virtAddr);

#endif /* SUPERVISOR_MM_EARLY_H */
