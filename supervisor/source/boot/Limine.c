#define DBG_MODULE "Limine"

#include <arch/Atomic.h>
#include <boot/Limine.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/MemMap.h>

[[gnu::used, gnu::section(".limine_requests")]]
volatile u64 Boot_LimineBaseRevision[] = LIMINE_BASE_REVISION(5);

[[gnu::used, gnu::section(".limine_requests_start")]]
static volatile u64 s_LimineStartMarker[] = LIMINE_REQUESTS_START_MARKER;
[[gnu::used, gnu::section(".limine_requests_end")]]
static volatile u64 s_LimineEndMarker[] = LIMINE_REQUESTS_END_MARKER;

[[gnu::used, gnu::section(".limine_requests")]]
static volatile struct limine_memmap_request s_MemmapReq = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
static volatile struct limine_hhdm_request s_HhdmReq = {.id = LIMINE_HHDM_REQUEST_ID, .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
static volatile struct limine_executable_address_request s_ExecAddrReq = {.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
                                                                          .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
static volatile struct limine_mp_request s_SmpReq = {.id = LIMINE_MP_REQUEST_ID, .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
static volatile struct limine_rsdp_request s_RsdpReq = {.id = LIMINE_RSDP_REQUEST_ID, .revision = 0};

#define MAX_MEMMAP_ENTRIES 256
#define MAX_CPUS           256

static Boot_MemEntry s_MemEntries[MAX_MEMMAP_ENTRIES];
static Boot_MemMap   s_MemMap;
static u32           s_ArchIds[MAX_CPUS];
static Boot_SmpInfo  s_SmpInfo;

static struct limine_mp_response *s_MpResponse = nullptr;

static Mm_RegionType s_TranslateMemType(u64 limineType)
{
    switch (limineType)
    {
        case LIMINE_MEMMAP_USABLE:
            return kMemTypeUsable;
        case LIMINE_MEMMAP_RESERVED:
            return kMemTypeReserved;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return kMemTypeACPIReclaimable;
        case LIMINE_MEMMAP_ACPI_NVS:
            return kMemTypeACPINVS;
        case LIMINE_MEMMAP_BAD_MEMORY:
            return kMemTypeBadMemory;
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return kMemTypeBootloaderReclaimable;
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            return kMemTypeSupervisorModules;
        case LIMINE_MEMMAP_FRAMEBUFFER:
            return kMemTypeFramebuffer;
        default:
            return kMemTypeReserved;
    }
}

bool Boot_Limine_Probe(void)
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(Boot_LimineBaseRevision))
        return false;

    if (!s_MemmapReq.response || !s_HhdmReq.response)
        return false;

    struct limine_memmap_response *mm    = s_MemmapReq.response;
    usize                          count = mm->entry_count;
    if (count > MAX_MEMMAP_ENTRIES)
        count = MAX_MEMMAP_ENTRIES;

    for (usize i = 0; i < count; i++)
    {
        s_MemEntries[i].Base   = mm->entries[i]->base;
        s_MemEntries[i].Length = mm->entries[i]->length;
        s_MemEntries[i].Type   = s_TranslateMemType(mm->entries[i]->type);
    }
    s_MemMap.Entries = s_MemEntries;
    s_MemMap.Count   = count;

    s_MpResponse = s_SmpReq.response;
    if (s_MpResponse && s_MpResponse->cpu_count > 1)
    {
        u32 cpuCount = s_MpResponse->cpu_count;
        if (cpuCount > MAX_CPUS)
            cpuCount = MAX_CPUS;

        s_SmpInfo.Count     = cpuCount;
        s_SmpInfo.BspArchId = s_MpResponse->bsp_lapic_id;
        s_SmpInfo.ArchIds   = s_ArchIds;

        for (u32 i = 0; i < cpuCount; i++)
            s_ArchIds[i] = s_MpResponse->cpus[i]->lapic_id;
    }

    return true;
}

u64 Boot_Limine_GetHhdmOffset(void)
{
    return s_HhdmReq.response->offset;
}

uptr Boot_Limine_GetRsdpPhys(void)
{
    struct limine_rsdp_response *resp = s_RsdpReq.response;
    if (!resp || !resp->address)
        return 0;

    const uptr hhdm     = s_HhdmReq.response->offset;
    const uptr rsdpVirt = (uptr)resp->address;
    return rsdpVirt - hhdm;
}

Boot_MemMap *Boot_Limine_GetMemMap(void)
{
    return &s_MemMap;
}

const Boot_SmpInfo *Boot_Limine_GetSmpInfo(void)
{
    if (!s_MpResponse || s_MpResponse->cpu_count <= 1)
        return nullptr;

    return &s_SmpInfo;
}

void Boot_Limine_SetCpuExtra(u32 cpuIndex, uptr extra)
{
    if (s_MpResponse && cpuIndex < s_MpResponse->cpu_count)
        s_MpResponse->cpus[cpuIndex]->extra_argument = extra;
}

static Boot_ApEntry s_GenericApEntry = nullptr;

[[noreturn]] static void s_LimineApTrampoline(struct limine_mp_info *info)
{
    s_GenericApEntry(info->extra_argument);
    unreachable();
}

void Boot_Limine_LaunchAps(Boot_ApEntry entry)
{
    if (!s_MpResponse)
        return;

    s_GenericApEntry = entry;

    for (u32 i = 0; i < s_MpResponse->cpu_count; i++)
    {
        if (s_MpResponse->cpus[i]->lapic_id == s_MpResponse->bsp_lapic_id)
            continue;

        Arch_AtomicStore64((Arch_Atomic64 *)&s_MpResponse->cpus[i]->goto_address, (uptr)s_LimineApTrampoline);
    }
}
