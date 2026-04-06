#define DBG_MODULE "Boot"

#include <boot/Limine.h>
#include <boot/Loader.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>

typedef enum
{
    kLoaderNone,
    kLoaderLimine,
} s_LoaderKind;

static s_LoaderKind s_Active = kLoaderNone;

bool Boot_Init(void)
{
    if (Boot_Limine_Probe())
    {
        s_Active = kLoaderLimine;
        Log(INFO, "detected Limine bootloader");
        return true;
    }

    return false;
}

u64 Boot_GetHhdmOffset(void)
{
    switch (s_Active)
    {
        case kLoaderLimine:
            return Boot_Limine_GetHhdmOffset();
        default:
            Panic("no bootloader");
    }
}

uptr Boot_GetRsdpPhys(void)
{
    switch (s_Active)
    {
        case kLoaderLimine:
            return Boot_Limine_GetRsdpPhys();
        default:
            return 0;
    }
}

Boot_MemMap *Boot_GetMemMap(void)
{
    switch (s_Active)
    {
        case kLoaderLimine:
            return Boot_Limine_GetMemMap();
        default:
            Panic("no bootloader");
    }
}

const Boot_SmpInfo *Boot_GetSmpInfo(void)
{
    switch (s_Active)
    {
        case kLoaderLimine:
            return Boot_Limine_GetSmpInfo();
        default:
            return nullptr;
    }
}

const Boot_ModuleList *Boot_GetModules(void)
{
    switch (s_Active)
    {
        case kLoaderLimine:
            return Boot_Limine_GetModules();
        default:
            return nullptr;
    }
}

void Boot_SetCpuExtra(u32 cpuIndex, uptr extra)
{
    switch (s_Active)
    {
        case kLoaderLimine:
            Boot_Limine_SetCpuExtra(cpuIndex, extra);
            return;
        default:
            Panic("no bootloader");
    }
}

void Boot_LaunchAps(Boot_ApEntry entry)
{
    switch (s_Active)
    {
        case kLoaderLimine:
            Boot_Limine_LaunchAps(entry);
            return;
        default:
            return;
    }
}
