#define DBG_MODULE "acpi"

#include <acpi/osi/OsServices.h>
#include <arch/Io.h>
#include <boot/Limine.h>
#include <core/VarArg.h>
#include <debug/DbgPrint.h>
#include <mm/Early.h>
#include <executive/Pool.h>
#include <mm/Vas.h>

#define EX_TAG_ACPI EX_TAG('A', 'c', 'p', 'i')

void Acpi_OsLog(Acpi_LogLevel level, const char *fmt, ...)
{
    static const Dbg_Level levelMap[] = {
        [kAcpiLogError] = DBG_ERROR, [kAcpiLogWarn] = DBG_WARN,   [kAcpiLogInfo] = DBG_INFO,
        [kAcpiLogDebug] = DBG_DEBUG, [kAcpiLogTrace] = DBG_TRACE,
    };

    Dbg_Level dbgLevel = (level <= kAcpiLogTrace) ? levelMap[level] : DBG_DEBUG;
    if (dbgLevel < Dbg_GetLevel())
        return;

    Core_VarArgs ap;
    Core_VarArgStart(ap);
    Dbg_VLogInner(dbgLevel, "acpi", fmt, ap);
    Core_VarArgEnd(ap);
}

Acpi_Vaddr Acpi_OsMap(Acpi_Paddr phys, Acpi_Usize length)
{
    if (length == 0)
        return nullptr;

    uptr va = Mm_MapIoSpace(phys, length, MM_CACHE_UNCACHED);
    return (va != 0) ? (Acpi_Vaddr)va : nullptr;
}

void Acpi_OsUnmap(Acpi_Vaddr virt, Acpi_Usize)
{
    if (virt)
        Mm_UnmapIoSpace((uptr)virt);
}

void *Acpi_OsAllocate(Acpi_Usize size)
{
    return Ex_Allocate(size, EX_TAG_ACPI);
}

void Acpi_OsFree(void *ptr)
{
    if (ptr)
        Ex_Free(ptr);
}

Acpi_Status Acpi_OsPortRead(Acpi_Uint16 port, unsigned width, Acpi_Uint64 *value)
{
    if (!value)
        return kAcpiInvalidArg;

    switch (width)
    {
        case 1:
            *value = Arch_IoIn8(port);
            return kAcpiOk;
        case 2:
            *value = Arch_IoIn16(port);
            return kAcpiOk;
        case 4:
            *value = Arch_IoIn32(port);
            return kAcpiOk;
        default:
            return kAcpiInvalidArg;
    }
}

Acpi_Status Acpi_OsPortWrite(Acpi_Uint16 port, unsigned width, Acpi_Uint64 value)
{
    switch (width)
    {
        case 1:
            Arch_IoOut8(port, value);
            return kAcpiOk;
        case 2:
            Arch_IoOut16(port, value);
            return kAcpiOk;
        case 4:
            Arch_IoOut32(port, value);
            return kAcpiOk;
        default:
            return kAcpiInvalidArg;
    }
}

Acpi_Status Acpi_OsMmioRead(Acpi_Vaddr addr, unsigned width, Acpi_Uint64 *value)
{
    if (!addr || !value)
        return kAcpiInvalidArg;

    switch (width)
    {
        case 1:
            *value = Arch_MmioRead8(addr);
            return kAcpiOk;
        case 2:
            *value = Arch_MmioRead16(addr);
            return kAcpiOk;
        case 4:
            *value = Arch_MmioRead32(addr);
            return kAcpiOk;
        case 8:
            *value = Arch_MmioRead64(addr);
            return kAcpiOk;
        default:
            return kAcpiInvalidArg;
    }
}

Acpi_Status Acpi_OsMmioWrite(Acpi_Vaddr addr, unsigned width, Acpi_Uint64 value)
{
    if (!addr)
        return kAcpiInvalidArg;

    switch (width)
    {
        case 1:
            Arch_MmioWrite8(addr, value);
            return kAcpiOk;
        case 2:
            Arch_MmioWrite16(addr, value);
            return kAcpiOk;
        case 4:
            Arch_MmioWrite32(addr, value);
            return kAcpiOk;
        case 8:
            Arch_MmioWrite64(addr, value);
            return kAcpiOk;
        default:
            return kAcpiInvalidArg;
    }
}

Acpi_Status Acpi_OsPciRead(Acpi_Uint16 seg, Acpi_Uint8 bus, Acpi_Uint8 dev, Acpi_Uint8 func, Acpi_Uint16 offset,
                           unsigned width, Acpi_Uint64 *value)
{
    (void)seg;
    (void)bus;
    (void)dev;
    (void)func;
    (void)offset;
    (void)width;
    (void)value;
    return kAcpiUnsupported;
}

Acpi_Status Acpi_OsPciWrite(Acpi_Uint16 seg, Acpi_Uint8 bus, Acpi_Uint8 dev, Acpi_Uint8 func, Acpi_Uint16 offset,
                            unsigned width, Acpi_Uint64 value)
{
    (void)seg;
    (void)bus;
    (void)dev;
    (void)func;
    (void)offset;
    (void)width;
    (void)value;
    return kAcpiUnsupported;
}

void Acpi_OsStall(Acpi_Uint64 us)
{
    (void)us;
    return;
}

Acpi_Status Acpi_OsSleep(Acpi_Uint64 ms)
{
    (void)ms;
    return kAcpiUnsupported;
}

Acpi_Status Acpi_OsMutexCreate(Acpi_OsMutex *out)
{
    (void)out;
    return kAcpiUnsupported;
}

void Acpi_OsMutexDestroy(Acpi_OsMutex mtx)
{
    (void)mtx;
}

Acpi_Status Acpi_OsMutexAcquire(Acpi_OsMutex mtx, Acpi_Uint64 timeoutMs)
{
    (void)mtx;
    (void)timeoutMs;
    return kAcpiUnsupported;
}

void Acpi_OsMutexRelease(Acpi_OsMutex mtx)
{
    (void)mtx;
}

Acpi_Status Acpi_OsInstallSciHandler(Acpi_Uint32 gsi, Acpi_SciHandler handler, void *ctx)
{
    (void)gsi;
    (void)handler;
    (void)ctx;
    return kAcpiUnsupported;
}

void Acpi_OsRemoveSciHandler(Acpi_Uint32 gsi, Acpi_SciHandler handler)
{
    (void)gsi;
    (void)handler;
}

Acpi_Paddr Acpi_OsGetRsdpAddress(void)
{
    struct limine_rsdp_response *resp = Boot_Limine_RsdpReq.response;
    if (!resp || !resp->address)
        return 0;

    const uptr hhdmOffset = Mm_GetHhdmBase();
    const uptr rsdpVirt   = (uptr)resp->address;

    return (Acpi_Paddr)(rsdpVirt - hhdmOffset);
}
