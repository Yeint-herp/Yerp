#ifndef SUPERVISOR_MM_VAD_H
#define SUPERVISOR_MM_VAD_H

#include <arch/MmArch.h>
#include <dsa/Avl.h>
#include <executive/Pool.h>

typedef enum Mm_VadType
{
    kVadTypePrivate,
    kVadTypeMmio,
    kVadTypeModule,
    kVadTypeReserved,
} Mm_VadType;

typedef enum Mm_VadFlags
{
    kVadFlagNone      = 0,
    kFlagCommitted = 1 << 0,
    kFlagGuard     = 1 << 1,
    kFlagZerod    = 1 << 2,
    kFlagFixed     = 1 << 3,
} Mm_VadFlags;

typedef struct Mm_Vad
{
    Dsa_AvlNode TreeNode;

    uptr  BaseAddress;
    usize RegionSize;

    Mm_VadType   Type;
    u32          Protection;
    Mm_CacheType CacheType;
    u32          Flags;

    uptr PhysicalBase;
} Mm_Vad;

typedef struct Mm_VadInfo
{
    uptr  BaseAddress;
    usize RegionSize;

    Mm_VadType   Type;
    u32          Protection;
    Mm_CacheType CacheType;
    u32          Flags;
} Mm_VadInfo;

i32 Mm_VadInsertCmp(Dsa_AvlNode *a, Dsa_AvlNode *b);
i32 Mm_VadContainsCmp(void *key, Dsa_AvlNode *node);
i32 Mm_VadBaseCmp(void *key, Dsa_AvlNode *node);

#define EX_TAG_VAD EX_TAG('M', 'm', 'V', 'd')

Mm_Vad *Mm_VadCreate(uptr base, usize size, Mm_VadType type, u32 prot, Mm_CacheType cache, u32 flags);
void    Mm_VadDestroy(Mm_Vad *vad);

#endif /* SUPERVISOR_MM_VAD_H */
