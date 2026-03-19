#ifndef SUPERVISOR_MM_EARLY_H
#define SUPERVISOR_MM_EARLY_H

#include <mm/MemMap.h>

struct limine_memmap_response;

void  Mm_EarlyInit(struct limine_memmap_response *mmResponse, u64 hhdmOffset);
void *Mm_EarlyAllocate(usize size, usize alignment);

const Mm_KernelMemMap *Mm_GetKernelMemMap(void);
u64                    Mm_GetHhdmBase(void);

#endif /* SUPERVISOR_MM_EARLY_H */
