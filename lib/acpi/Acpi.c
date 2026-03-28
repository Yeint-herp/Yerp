#include <acpi/Acpi.h>
#include <acpi/osi/OsServices.h>
#include <acpi/tables/Fadt.h>
#include <acpi/tables/Tables.h>

static Acpi_TableContext s_TableCtx  = {};
static Acpi_FadtInfo     s_FadtInfo  = {};
static bool              s_EarlyDone = false;

const Acpi_SdtHeader *Acpi_FindTable(Acpi_Uint32 signature, Acpi_Usize index)
{
    if (!s_EarlyDone)
        return nullptr;

    return Acpi_TablesFind(&s_TableCtx, signature, index);
}

const Acpi_FadtInfo *Acpi_GetFadtInfo(void)
{
    return s_EarlyDone ? &s_FadtInfo : nullptr;
}

Acpi_Status Acpi_EarlyInit(void)
{
    if (s_EarlyDone)
        return kAcpiAlreadyExists;

    Acpi_OsLog(kAcpiLogInfo, "early init");

    Acpi_Status st = Acpi_TablesInit(&s_TableCtx);
    if (Acpi_Failure(st))
        return st;

    s_EarlyDone = true;

    const Acpi_Fadt *fadt = Acpi_FadtGet();
    if (fadt)
    {
        st = Acpi_FadtParse(fadt, &s_FadtInfo);
        if (Acpi_Failure(st))
            Acpi_OsLog(kAcpiLogWarn, "FADT parse failed (%d)", st);
    }
    else
        Acpi_OsLog(kAcpiLogWarn, "no FADT found");

    return kAcpiOk;
}

Acpi_Status Acpi_Init(void)
{
    return kAcpiOk;
}
