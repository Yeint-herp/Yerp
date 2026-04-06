#define DBG_MODULE "IoDriver"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <executive/Pool.h>
#include <io/Device.h>
#include <io/Driver.h>
#include <io/Io.h>

static i32 IoDispatchInvalid(Io_Device *device, Io_Irp *irp)
{
    (void)device;
    Io_CompleteRequest(irp, kIoNotSupported, 0);
    return kIoNotSupported;
}

i32 Io_CreateDriver(const char *name, Io_DriverEntry entry, usize extensionSize, Io_Driver **outDriver)
{
    if (!name || !entry || !outDriver)
        return kIoInvalidParameter;

    Io_Driver *driver = nullptr;
    i32        status = Ob_CreateObject(Io_GetDriverType(), 0, (void **)&driver);
    if (status != kObSuccess)
        return kIoInsufficientResources;

    Core_ZeroMemory(driver, sizeof *driver);

    driver->Name        = name;
    driver->DriverEntry = entry;

    Dsa_ListInit(&driver->DeviceListHead);
    Core_SpinlockInit(&driver->DeviceListLock);

    for (u32 i = 0; i < IRP_MJ_COUNT; i++)
        driver->DispatchTable[i] = IoDispatchInvalid;

    if (extensionSize > 0)
    {
        driver->DriverExtension = Ex_Allocate(extensionSize, IO_TAG_DRIVER);
        if (!driver->DriverExtension)
        {
            Ob_DereferenceObject(driver);
            return kIoInsufficientResources;
        }

        Core_ZeroMemory(driver->DriverExtension, extensionSize);
        driver->DriverExtensionSize = extensionSize;
    }

    status = driver->DriverEntry(driver);
    if (status != kIoSuccess)
    {
        if (driver->DriverExtension)
            Ex_Free(driver->DriverExtension);

        Ob_DereferenceObject(driver);
        return status;
    }

    Log(INFO, "loaded driver '%s'", name);

    *outDriver = driver;
    return kIoSuccess;
}

void Io_DeleteDriver(Io_Driver *driver)
{
    if (!driver)
        return;

    if (driver->DriverUnload)
        driver->DriverUnload(driver);

    if (driver->DriverExtension)
    {
        Ex_Free(driver->DriverExtension);
        driver->DriverExtension = nullptr;
    }

    Log(INFO, "unloaded driver '%s'", driver->Name);

    Ob_DereferenceObject(driver);
}

void *Io_GetDriverExtension(Io_Driver *driver)
{
    return driver->DriverExtension;
}
