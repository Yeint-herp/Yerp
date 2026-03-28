#include <acpi/AcpiTypes.h>
#include <acpi/osi/OsServices.h>
#include <acpi/tables/Tables.h>

Acpi_Status Acpi_ValidateChecksum(const void *data, Acpi_Usize length)
{
    const Acpi_Uint8 *bytes = data;
    Acpi_Uint8        sum   = 0;

    for (Acpi_Usize i = 0; i < length; i++)
        sum += bytes[i];

    return sum == 0 ? kAcpiOk : kAcpiBadChecksum;
}

Acpi_Status Acpi_ValidateRsdp(const Acpi_Rsdp *rsdp)
{
    Acpi_Status st = Acpi_ValidateChecksum(rsdp, 20);
    if (Acpi_Failure(st))
        return st;

    if (rsdp->Revision >= 2)
    {
        st = Acpi_ValidateChecksum(rsdp, rsdp->Length);
        if (Acpi_Failure(st))
            return st;
    }

    return kAcpiOk;
}

static const Acpi_SdtHeader *s_MapAndValidateTable(Acpi_Paddr phys)
{
    Acpi_Vaddr hdrMap = Acpi_OsMap(phys, sizeof(Acpi_SdtHeader));
    if (!hdrMap)
        return nullptr;

    const Acpi_SdtHeader *hdr    = hdrMap;
    const Acpi_Uint32     length = hdr->Length;

    if (length < sizeof(Acpi_SdtHeader))
    {
        Acpi_OsUnmap(hdrMap, sizeof(Acpi_SdtHeader));
        return nullptr;
    }

    if (length == sizeof(Acpi_SdtHeader))
    {
        if (Acpi_Failure(Acpi_ValidateChecksum(hdr, length)))
        {
            Acpi_OsUnmap(hdrMap, sizeof(Acpi_SdtHeader));
            return nullptr;
        }

        return hdr;
    }

    Acpi_OsUnmap(hdrMap, sizeof(Acpi_SdtHeader));

    Acpi_Vaddr fullMap = Acpi_OsMap(phys, length);
    if (!fullMap)
        return nullptr;

    const Acpi_SdtHeader *full = fullMap;

    if (Acpi_Failure(Acpi_ValidateChecksum(full, length)))
    {
        Acpi_OsLog(kAcpiLogError, "bad checksum for table %.4s at %p", (const char *)&full->Signature, (void *)phys);
        Acpi_OsUnmap(fullMap, length);
        return nullptr;
    }

    return full;
}

Acpi_Status Acpi_TablesInit(Acpi_TableContext *ctx)
{
    const Acpi_Paddr rsdpAddr = Acpi_OsGetRsdpAddress();
    if (rsdpAddr == 0)
    {
        Acpi_OsLog(kAcpiLogError, "no RSDP address from OS");
        return kAcpiNotFound;
    }

    Acpi_Paddr rootAddr;
    {
        const Acpi_Vaddr rsdpMap = Acpi_OsMap(rsdpAddr, sizeof(Acpi_Rsdp));
        if (!rsdpMap)
            return kAcpiNoMemory;

        const Acpi_Rsdp *rsdp = rsdpMap;

        const Acpi_Status st = Acpi_ValidateRsdp(rsdp);
        if (Acpi_Failure(st))
        {
            Acpi_OsLog(kAcpiLogError, "RSDP validation failed (%d)", st);

            Acpi_OsUnmap(rsdpMap, sizeof(Acpi_Rsdp));
            return st;
        }

        ctx->Rsdp    = *rsdp;
        ctx->UseXsdt = (rsdp->Revision >= 2 && rsdp->XsdtAddress != 0);
        ctx->Count   = 0;

        rootAddr = ctx->UseXsdt ? (Acpi_Paddr)rsdp->XsdtAddress : (Acpi_Paddr)rsdp->RsdtAddress;

        Acpi_OsUnmap(rsdpMap, sizeof(Acpi_Rsdp));
    }

    Acpi_OsLog(kAcpiLogInfo, "using %s at %p (revision %u)", ctx->UseXsdt ? "XSDT" : "RSDT", (void *)rootAddr,
               ctx->Rsdp.Revision);

    const Acpi_SdtHeader *root = s_MapAndValidateTable(rootAddr);
    if (!root)
    {
        Acpi_OsLog(kAcpiLogError, "failed to map root SDT");
        return kAcpiBadHeader;
    }

    const Acpi_Usize entrySize  = ctx->UseXsdt ? sizeof(Acpi_Uint64) : sizeof(Acpi_Uint32);
    const Acpi_Usize dataLen    = root->Length - sizeof(Acpi_SdtHeader);
    const Acpi_Usize numEntries = dataLen / entrySize;

    const Acpi_Uint8 *entryBase = (const Acpi_Uint8 *)root + sizeof(Acpi_SdtHeader);
    for (Acpi_Usize i = 0; i < numEntries && ctx->Count < ACPI_MAX_TABLES; i++)
    {
        Acpi_Paddr tableAddr;

        if (ctx->UseXsdt)
        {
            Acpi_Uint64 raw;
            __builtin_memcpy(&raw, entryBase + i * entrySize, sizeof(raw));
            tableAddr = (Acpi_Paddr)raw;
        }
        else
        {
            Acpi_Uint32 raw;
            __builtin_memcpy(&raw, entryBase + i * entrySize, sizeof(raw));
            tableAddr = (Acpi_Paddr)raw;
        }

        if (tableAddr == 0)
            continue;

        Acpi_Vaddr peek = Acpi_OsMap(tableAddr, sizeof(Acpi_SdtHeader));
        if (!peek)
            continue;

        const Acpi_SdtHeader *hdr   = peek;
        Acpi_TableEntry      *entry = &ctx->Entries[ctx->Count];

        entry->Signature = hdr->Signature;
        entry->PhysAddr  = tableAddr;
        entry->Mapped    = nullptr;

        Acpi_OsLog(kAcpiLogDebug, "found table %.4s at %p", (const char *)&hdr->Signature, (void *)tableAddr);

        Acpi_OsUnmap(peek, sizeof(Acpi_SdtHeader));
        ctx->Count++;
    }
    Acpi_OsLog(kAcpiLogInfo, "cached %zu table entries", ctx->Count);

    Acpi_OsUnmap((Acpi_Vaddr)root, root->Length);
    return kAcpiOk;
}

const Acpi_SdtHeader *Acpi_TablesFind(Acpi_TableContext *ctx, Acpi_Uint32 signature, Acpi_Usize index)
{
    Acpi_Usize match = 0;
    for (Acpi_Usize i = 0; i < ctx->Count; i++)
    {
        Acpi_TableEntry *entry = &ctx->Entries[i];

        if (entry->Signature != signature)
            continue;

        if (match == index)
        {
            if (!entry->Mapped)
            {
                entry->Mapped = s_MapAndValidateTable(entry->PhysAddr);
                if (!entry->Mapped)
                {
                    Acpi_OsLog(kAcpiLogError,
                               "failed to map table "
                               "%.4s[%zu] at %p",
                               (const char *)&signature, index, (void *)entry->PhysAddr);

                    return nullptr;
                }
            }

            return entry->Mapped;
        }
        match++;
    }

    return nullptr;
}
