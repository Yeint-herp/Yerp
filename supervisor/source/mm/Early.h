#ifndef SUPERVISOR_MM_EARLY_H
#define SUPERVISOR_MM_EARLY_H

#include <mm/MemMap.h>

struct limine_memmap_response;

void  Mm_EarlyInit(struct limine_memmap_response *mmResponse, u64 hhdmOffset);
void *Mm_PermanentAllocate(usize size, usize alignment);

void Mm_SetPfnReady(void);
bool Mm_IsPfnReady(void);

const Mm_SupervisorMemMap *Mm_GetSupervisorMemMap(void);
u64                        Mm_GetHhdmBase(void);

void *Mm_PhysToVirt(uptr physAddr);
uptr  Mm_VirtToPhys(void *virtAddr);

#endif /* SUPERVISOR_MM_EARLY_H */
