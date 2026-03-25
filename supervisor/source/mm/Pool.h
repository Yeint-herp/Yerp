#ifndef SUPERVISOR_MM_POOL_H
#define SUPERVISOR_MM_POOL_H

#define EX_TAG(a, b, c, d) ((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

#define EX_TAG_GENERIC EX_TAG('E', 'x', 'G', 'n')
#define EX_TAG_OBJECT  EX_TAG('O', 'b', 'j', 'H')
#define EX_TAG_PROCESS EX_TAG('P', 's', 'P', 'r')

void *Ex_Allocate(usize size, u32 tag);
void  Ex_Free(void *ptr);

void Ex_PoolInit(void);

#endif /* SUPERVISOR_MM_POOL_H */
