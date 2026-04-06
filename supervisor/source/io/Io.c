#define DBG_MODULE "Io"

#include <debug/DbgPrint.h>
#include <io/Device.h>
#include <io/Driver.h>
#include <io/File.h>
#include <io/Io.h>

static Ob_Type *s_DriverType;
static Ob_Type *s_DeviceType;
static Ob_Type *s_FileType;

static void IoDeviceDelete(void *object);
static void IoFileDelete(void *object);
static void IoFileClose(void *object, usize handleCount);

void Io_SystemInit(void)
{
    static const Ob_TypeInfo driverInfo = {
        .Name            = "Driver",
        .ObjectBodySize  = sizeof(Io_Driver),
        .DeleteProcedure = nullptr,
        .CloseProcedure  = nullptr,
        .PoolTag         = IO_TAG_DRIVER,
        .ValidAccessMask = 0,
    };

    static const Ob_TypeInfo deviceInfo = {
        .Name            = "Device",
        .ObjectBodySize  = sizeof(Io_Device),
        .DeleteProcedure = IoDeviceDelete,
        .CloseProcedure  = nullptr,
        .PoolTag         = IO_TAG_DEVICE,
        .ValidAccessMask = 0,
    };

    static const Ob_TypeInfo fileInfo = {
        .Name            = "File",
        .ObjectBodySize  = sizeof(Io_File),
        .DeleteProcedure = IoFileDelete,
        .CloseProcedure  = IoFileClose,
        .PoolTag         = IO_TAG_FILE,
        .ValidAccessMask = IO_FILE_ALL_ACCESS,
    };

    s_DriverType = Ob_CreateType(&driverInfo);
    s_DeviceType = Ob_CreateType(&deviceInfo);
    s_FileType   = Ob_CreateType(&fileInfo);

    Ob_CreateDirectory("\\Device", nullptr);

    Log(INFO, "I/O manager initialized");
}

Ob_Type *Io_GetDriverType(void)
{
    return s_DriverType;
}

Ob_Type *Io_GetDeviceType(void)
{
    return s_DeviceType;
}

Ob_Type *Io_GetFileType(void)
{
    return s_FileType;
}

static void IoDeviceDelete(void *object)
{
    Io_Device *dev = object;

    if (dev->DeviceName)
    {
        Ob_RemoveObjectByPath(dev->DeviceName);
        Ex_Free(dev->DeviceName);
    }

    if (dev->DeviceExtension)
        Ex_Free(dev->DeviceExtension);
}

static void IoFileDelete(void *object)
{
    (void)object;
}

static void IoFileClose(void *object, usize handleCount)
{
    if (handleCount != 0)
        return;

    Io_File *file = object;

    Io_Device *device = file->Device;
    if (!device)
        return;

    Io_Device *top = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(top->StackSize);
    if (!irp)
        return;

    Io_StackLocation *sl = Io_GetNextStackLocation(irp);
    sl->MajorFunction    = IRP_MJ_CLOSE;
    sl->FileObject       = file;
    sl->Device           = top;

    Io_CallDriver(top, irp);
    Io_FreeIrp(irp);
}
