#include <acpi/Acpi.h>
#include <acpi/AcpiTypes.h>
#include <acpi/osi/OsServices.h>
#include <acpi/tables/GenericEntry.h>

static unsigned s_GasAccessWidth(const Acpi_Gas *gas)
{
    if (gas->AccessSize != kAcpiAccessUndefined)
        return 1u << (gas->AccessSize - 1);

    unsigned bits = gas->RegisterBitWidth;
    if (bits <= 8)
        return 1;
    else if (bits <= 16)
        return 2;
    else if (bits <= 32)
        return 4;

    return 8;
}

Acpi_Status Acpi_GasRead(const Acpi_Gas *gas, Acpi_Uint64 *value)
{
    if (!gas || !value)
        return kAcpiInvalidArg;

    if (gas->Address == 0)
        return kAcpiNotFound;

    unsigned width = s_GasAccessWidth(gas);

    switch (gas->AddressSpaceId)
    {
        case kAcpiAddrSpaceIo:
            return Acpi_OsPortRead(gas->Address, width, value);

        case kAcpiAddrSpaceMemory:
        {
            Acpi_Vaddr virt = Acpi_OsMap(gas->Address, width);
            if (!virt)
                return kAcpiNoMemory;

            Acpi_Status st = Acpi_OsMmioRead(virt, width, value);
            Acpi_OsUnmap(virt, width);

            if (Acpi_Success(st) && gas->RegisterBitOffset != 0)
                *value >>= gas->RegisterBitOffset;
            if (Acpi_Success(st) && gas->RegisterBitWidth < 64)
                *value &= ((Acpi_Uint64)1 << gas->RegisterBitWidth) - 1;

            return st;
        }

        default:
            Acpi_OsLog(kAcpiLogWarn, "unsupported GAS address space %#02x", gas->AddressSpaceId);
            return kAcpiUnsupported;
    }
}

Acpi_Status Acpi_GasWrite(const Acpi_Gas *gas, Acpi_Uint64 value)
{
    if (!gas)
        return kAcpiInvalidArg;

    if (gas->Address == 0)
        return kAcpiNotFound;

    unsigned width = s_GasAccessWidth(gas);

    switch (gas->AddressSpaceId)
    {
        case kAcpiAddrSpaceIo:
            return Acpi_OsPortWrite(gas->Address, width, value);

        case kAcpiAddrSpaceMemory:
        {
            Acpi_Vaddr virt = Acpi_OsMap(gas->Address, width);
            if (!virt)
                return kAcpiNoMemory;

            if (gas->RegisterBitOffset != 0)
                value <<= gas->RegisterBitOffset;

            Acpi_Status st = Acpi_OsMmioWrite(virt, width, value);
            Acpi_OsUnmap(virt, width);
            return st;
        }

        default:
            Acpi_OsLog(kAcpiLogWarn, "unsupported GAS address space %#02x", gas->AddressSpaceId);
            return kAcpiUnsupported;
    }
}
