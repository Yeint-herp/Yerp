#define DBG_MODULE "CfmPop"

#include <acpi/Acpi.h>
#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <executive/CfmPopulate.h>

static i32 s_EnsureKey(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle parent, const char *name, Ob_Handle *out)
{
    i32 st = Cf_CreateKey(t, tok, parent, name, CF_KEY_ALL_ACCESS, out);
    if (st == kCfNameCollision)
        st = Cf_OpenKey(t, tok, parent, name, CF_KEY_ALL_ACCESS, out);

    return st;
}

static i32 s_EnsurePath(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle root, const char *path, Ob_Handle *outLeaf)
{
    Ob_Handle current     = root;
    bool      ownsCurrent = false;

    while (*path)
    {
        while (*path == '\\')
            path++;

        if (*path == '\0')
            break;

        const char *end = path;
        while (*end && *end != '\\')
            end++;

        usize len = end - path;
        if (len == 0 || len >= OB_MAX_COMPONENT)
            return kCfInvalidParameter;

        char component[OB_MAX_COMPONENT];
        Core_CopyMemory(component, path, len);
        component[len] = '\0';

        Ob_Handle child;
        i32       st = s_EnsureKey(t, tok, current, component, &child);

        if (ownsCurrent)
            Ob_CloseHandle(t, current);

        if (st != kCfSuccess)
            return st;

        current     = child;
        ownsCurrent = true;
        path        = end;
    }

    if (!ownsCurrent)
        return Cf_OpenKey(t, tok, root, "", CF_KEY_ALL_ACCESS, outLeaf);

    *outLeaf = current;
    return kCfSuccess;
}

static i32 s_SetU32(Ob_HandleTable *t, Ob_Handle key, const char *name, u32 val)
{
    return Cf_SetValue(t, key, name, kCfTypeU32, &val, sizeof val);
}

static i32 s_SetU64(Ob_HandleTable *t, Ob_Handle key, const char *name, u64 val)
{
    return Cf_SetValue(t, key, name, kCfTypeU64, &val, sizeof val);
}

static void s_PopulateFadt(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle hwKey)
{
    const Acpi_FadtInfo *info = Acpi_GetFadtInfo();
    if (!info)
    {
        Log(WARN, "no FADT info available, skipping");
        return;
    }

    Ob_Handle fadtKey;
    i32       st = s_EnsurePath(t, tok, hwKey, "Acpi\\Fadt", &fadtKey);
    if (st != kCfSuccess)
    {
        Log(ERROR, "failed to create Hardware\\Acpi\\Fadt (%d)", st);
        return;
    }

    s_SetU32(t, fadtKey, "HwReduced", info->HwReduced);
    s_SetU32(t, fadtKey, "HasCmosRtc", info->HasCmosRtc);
    s_SetU32(t, fadtKey, "Has8042", info->Has8042);
    s_SetU32(t, fadtKey, "SciGsi", info->SciGsi);
    s_SetU64(t, fadtKey, "DsdtAddress", info->DsdtAddress);
    s_SetU64(t, fadtKey, "FacsAddress", info->FacsAddress);
    s_SetU32(t, fadtKey, "PmProfile", info->PmProfile);
    s_SetU32(t, fadtKey, "BootArch", info->BootArch);
    s_SetU32(t, fadtKey, "Flags", info->Flags);

    Ob_CloseHandle(t, fadtKey);
    Log(INFO, "populated Hardware\\Acpi\\Fadt");
}

#if KArch == x86_64

#include <arch/x86_64/Apic.h>

static void s_PopulateLapics(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle apicKey, const X86_64_ApicState *state)
{
    Ob_Handle lapicRoot;
    if (s_EnsureKey(t, tok, apicKey, "Lapic", &lapicRoot) != kCfSuccess)
        return;

    usize count = Dsa_VectorCount(state->Lapics);
    for (usize i = 0; i < count; i++)
    {
        char name[32];
        Core_UnsignedToString(name, i, 10);

        Ob_Handle entry;
        if (s_EnsureKey(t, tok, lapicRoot, name, &entry) != kCfSuccess)
            continue;

        const X86_64_ApicLapicInfo *lap = &state->Lapics[i];

        s_SetU32(t, entry, "ApicId", lap->ApicId);
        s_SetU32(t, entry, "AcpiProcessorId", lap->AcpiProcessorId);
        s_SetU32(t, entry, "Flags", lap->Flags);

        Ob_CloseHandle(t, entry);
    }

    s_SetU32(t, lapicRoot, "Count", count);
    Ob_CloseHandle(t, lapicRoot);
}

static void s_PopulateIoApics(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle apicKey, const X86_64_ApicState *state)
{
    Ob_Handle ioRoot;
    if (s_EnsureKey(t, tok, apicKey, "IoApic", &ioRoot) != kCfSuccess)
        return;

    usize count = Dsa_VectorCount(state->IoApics);
    for (usize i = 0; i < count; i++)
    {
        char name[32];
        Core_UnsignedToString(name, i, 10);

        Ob_Handle entry;
        if (s_EnsureKey(t, tok, ioRoot, name, &entry) != kCfSuccess)
            continue;

        const X86_64_ApicIoApicInfo *io = &state->IoApics[i];

        s_SetU32(t, entry, "IoApicId", io->IoApicId);
        s_SetU32(t, entry, "GsiBase", io->GsiBase);
        s_SetU32(t, entry, "GsiCount", io->GsiCount);
        s_SetU64(t, entry, "PhysBase", io->PhysBase);

        Ob_CloseHandle(t, entry);
    }

    s_SetU32(t, ioRoot, "Count", count);
    Ob_CloseHandle(t, ioRoot);
}

static void s_PopulateIsos(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle apicKey, const X86_64_ApicState *state)
{
    Ob_Handle isoRoot;
    if (s_EnsureKey(t, tok, apicKey, "Iso", &isoRoot) != kCfSuccess)
        return;

    usize count = Dsa_VectorCount(state->Isos);
    for (usize i = 0; i < count; i++)
    {
        char name[32];
        Core_UnsignedToString(name, i, 10);

        Ob_Handle entry;
        if (s_EnsureKey(t, tok, isoRoot, name, &entry) != kCfSuccess)
            continue;

        const X86_64_ApicIsoInfo *iso = &state->Isos[i];

        s_SetU32(t, entry, "Bus", iso->Bus);
        s_SetU32(t, entry, "Source", iso->Source);
        s_SetU32(t, entry, "Gsi", iso->Gsi);
        s_SetU32(t, entry, "Flags", iso->Flags);

        Ob_CloseHandle(t, entry);
    }

    s_SetU32(t, isoRoot, "Count", count);
    Ob_CloseHandle(t, isoRoot);
}

static void s_PopulateNmis(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle apicKey, const X86_64_ApicState *state)
{
    Ob_Handle nmiRoot;
    if (s_EnsureKey(t, tok, apicKey, "Nmi", &nmiRoot) != kCfSuccess)
        return;

    usize count = Dsa_VectorCount(state->Nmis);
    for (usize i = 0; i < count; i++)
    {
        char name[32];
        Core_UnsignedToString(name, i, 10);

        Ob_Handle entry;
        if (s_EnsureKey(t, tok, nmiRoot, name, &entry) != kCfSuccess)
            continue;

        const X86_64_ApicNmiInfo *nmi = &state->Nmis[i];

        s_SetU32(t, entry, "Lint", nmi->Lint);
        s_SetU32(t, entry, "Flags", nmi->Flags);

        Ob_CloseHandle(t, entry);
    }

    s_SetU32(t, nmiRoot, "Count", count);
    Ob_CloseHandle(t, nmiRoot);
}

static void s_PopulateApic(Ob_HandleTable *t, Acl_Token *tok, Ob_Handle hwKey)
{
    const X86_64_ApicState *state = X86_64_ApicGetState();
    if (!state)
    {
        Log(WARN, "no APIC state available, skipping");
        return;
    }

    Ob_Handle apicKey;
    i32       st = s_EnsureKey(t, tok, hwKey, "Apic", &apicKey);
    if (st != kCfSuccess)
    {
        Log(ERROR, "failed to create Hardware\\Apic (%d)", st);
        return;
    }

    s_SetU64(t, apicKey, "LapicBase", state->LapicBase);
    s_SetU32(t, apicKey, "X2ApicMode", state->x2apic);

    s_PopulateLapics(t, tok, apicKey, state);
    s_PopulateIoApics(t, tok, apicKey, state);
    s_PopulateIsos(t, tok, apicKey, state);
    s_PopulateNmis(t, tok, apicKey, state);

    Ob_CloseHandle(t, apicKey);
    Log(INFO, "populated Hardware\\Apic");
}

#endif /* KArch == x86_64 */

void Cf_PopulateHardware(Ob_HandleTable *table, Acl_Token *token)
{
    Ob_Handle hwKey;
    i32       st = s_EnsureKey(table, token, OB_HANDLE_NULL, "Hardware", &hwKey);
    if (st != kCfSuccess)
    {
        Log(ERROR, "failed to create Hardware key (%d)", st);
        return;
    }

    s_PopulateFadt(table, token, hwKey);

#if KArch == x86_64
    s_PopulateApic(table, token, hwKey);
#endif

    Ob_CloseHandle(table, hwKey);
}
