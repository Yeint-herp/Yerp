#define DBG_MODULE "IoFile"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <dispatcher/Dispatcher.h>
#include <executive/Pool.h>
#include <io/Device.h>
#include <io/Driver.h>
#include <io/File.h>
#include <io/Io.h>
#include <io/Irp.h>

i32 Io_CreateFile(Ob_HandleTable *table, Acl_Token *token, const char *path, u32 desiredAccess, u32 shareAccess,
                  u32 createFlags, Ob_Handle *outHandle)
{
    if (!table || !path || !outHandle)
        return kIoInvalidParameter;

    Io_Device *device = nullptr;
    i32        status = Ob_LookupObjectByPath(path, Io_GetDeviceType(), (void **)&device);
    if (status != kObSuccess)
        return kIoNotFound;

    Io_File *file = nullptr;
    status        = Ob_CreateObject(Io_GetFileType(), 0, (void **)&file);
    if (status != kObSuccess)
    {
        Ob_DereferenceObject(device);
        return kIoInsufficientResources;
    }

    Core_ZeroMemory(file, sizeof(*file));

    file->Device      = device;
    file->ShareAccess = shareAccess;
    file->Flags       = 0;

    if (createFlags & IO_FILE_SYNCHRONOUS)
        file->Flags |= IO_FILE_SYNCHRONOUS;

    Ds_EventInit(&file->Event, kDsObjectNotificationEvent, false);
    Core_SpinlockInit(&file->Lock);

    Io_Device *topDevice = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(topDevice->StackSize);
    if (!irp)
    {
        Ob_DereferenceObject(file);
        Ob_DereferenceObject(device);
        return kIoInsufficientResources;
    }

    Io_StackLocation *sl = Io_GetNextStackLocation(irp);
    sl->MajorFunction    = IRP_MJ_CREATE;
    sl->FileObject       = file;
    sl->Flags            = 0;

    irp->Flags |= IRP_FLAG_SYNCHRONOUS;

    status = Io_CallDriver(topDevice, irp);
    if (status == kIoPending)
    {
        Ds_WaitForObject(&irp->CompletionEvent.Header, DS_TIMEOUT_INFINITE);
        status = irp->Status;
    }

    if (status != kIoSuccess)
    {
        Io_FreeIrp(irp);
        Ob_DereferenceObject(file);
        Ob_DereferenceObject(device);
        return status;
    }

    Io_FreeIrp(irp);

    Ob_Handle handle   = OB_HANDLE_NULL;
    i32       obStatus = Ob_InsertHandle(table, file, desiredAccess, token, &handle);
    if (obStatus != kObSuccess)
    {
        Ob_DereferenceObject(file);
        Ob_DereferenceObject(device);
        return kIoAccessDenied;
    }

    *outHandle = handle;
    return kIoSuccess;
}

static i32 IoResolveFile(Ob_HandleTable *table, Ob_Handle handle, Io_File **outFile)
{
    void *obj    = nullptr;
    i32   status = Ob_ReferenceByHandle(table, handle, Io_GetFileType(), &obj);
    if (status != kObSuccess)
        return kIoInvalidParameter;

    *outFile = obj;
    return kIoSuccess;
}

static i32 IoReadWriteCommon(Ob_HandleTable *table, Ob_Handle fileHandle, void *buffer, usize length, u64 *offset,
                             usize *transferred, u8 majorFunction)
{
    Io_File *file   = nullptr;
    i32      status = IoResolveFile(table, fileHandle, &file);
    if (status != kIoSuccess)
        return status;

    Io_Device *device    = file->Device;
    Io_Device *topDevice = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(topDevice->StackSize);
    if (!irp)
    {
        Ob_DereferenceObject(file);
        return kIoInsufficientResources;
    }

    u64 fileOffset;
    if (offset)
        fileOffset = *offset;
    else
        fileOffset = file->CurrentOffset;

    Io_StackLocation *sl            = Io_GetNextStackLocation(irp);
    sl->MajorFunction               = majorFunction;
    sl->FileObject                  = file;
    sl->Parameters.ReadWrite.Length = length;
    sl->Parameters.ReadWrite.Offset = fileOffset;

    irp->UserBuffer = buffer;

    if (device->Flags & IO_DEVICE_BUFFERED_IO)
    {
        irp->Flags |= IRP_FLAG_BUFFERED;
        irp->SystemBuffer = Ex_Allocate(length, IO_TAG_IRP);
        if (!irp->SystemBuffer)
        {
            Io_FreeIrp(irp);
            Ob_DereferenceObject(file);
            return kIoInsufficientResources;
        }

        if (majorFunction == IRP_MJ_WRITE)
            Core_CopyMemory(irp->SystemBuffer, buffer, length);
        else
            Core_ZeroMemory(irp->SystemBuffer, length);
    }

    irp->Flags |= IRP_FLAG_SYNCHRONOUS;

    status = Io_CallDriver(topDevice, irp);
    if (status == kIoPending)
    {
        Ds_WaitForObject(&irp->CompletionEvent.Header, DS_TIMEOUT_INFINITE);
        status = irp->Status;
    }

    usize done = irp->Information;

    if (!offset && status == kIoSuccess)
        file->CurrentOffset += done;

    if (transferred)
        *transferred = done;

    Io_FreeIrp(irp);
    Ob_DereferenceObject(file);
    return status;
}

i32 Io_ReadFile(Ob_HandleTable *table, Ob_Handle fileHandle, void *buffer, usize length, u64 *offset, usize *bytesRead)
{
    return IoReadWriteCommon(table, fileHandle, buffer, length, offset, bytesRead, IRP_MJ_READ);
}

i32 Io_WriteFile(Ob_HandleTable *table, Ob_Handle fileHandle, const void *buffer, usize length, u64 *offset,
                 usize *bytesWritten)
{
    return IoReadWriteCommon(table, fileHandle, (void *)buffer, length, offset, bytesWritten, IRP_MJ_WRITE);
}

i32 Io_DeviceIoControl(Ob_HandleTable *table, Ob_Handle fileHandle, u32 controlCode, const void *inBuffer,
                       usize inLength, void *outBuffer, usize outLength, usize *bytesReturned)
{
    Io_File *file   = nullptr;
    i32      status = IoResolveFile(table, fileHandle, &file);
    if (status != kIoSuccess)
        return status;

    Io_Device *device    = file->Device;
    Io_Device *topDevice = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(topDevice->StackSize);
    if (!irp)
    {
        Ob_DereferenceObject(file);
        return kIoInsufficientResources;
    }

    Io_StackLocation *sl                      = Io_GetNextStackLocation(irp);
    sl->MajorFunction                         = IRP_MJ_DEVICE_CONTROL;
    sl->FileObject                            = file;
    sl->Parameters.DeviceControl.ControlCode  = controlCode;
    sl->Parameters.DeviceControl.InputLength  = inLength;
    sl->Parameters.DeviceControl.OutputLength = outLength;
    sl->Parameters.DeviceControl.InputBuffer  = (void *)inBuffer;

    irp->UserBuffer = outBuffer;

    if (device->Flags & IO_DEVICE_BUFFERED_IO)
    {
        usize allocSize = (inLength > outLength) ? inLength : outLength;
        if (allocSize > 0)
        {
            irp->SystemBuffer = Ex_Allocate(allocSize, IO_TAG_IRP);
            if (!irp->SystemBuffer)
            {
                Io_FreeIrp(irp);
                Ob_DereferenceObject(file);
                return kIoInsufficientResources;
            }
            Core_ZeroMemory(irp->SystemBuffer, allocSize);

            if (inLength > 0)
                Core_CopyMemory(irp->SystemBuffer, inBuffer, inLength);
        }
        irp->Flags |= IRP_FLAG_BUFFERED;
    }

    irp->Flags |= IRP_FLAG_SYNCHRONOUS;

    status = Io_CallDriver(topDevice, irp);
    if (status == kIoPending)
    {
        Ds_WaitForObject(&irp->CompletionEvent.Header, DS_TIMEOUT_INFINITE);
        status = irp->Status;
    }

    if (bytesReturned)
        *bytesReturned = irp->Information;

    Io_FreeIrp(irp);
    Ob_DereferenceObject(file);
    return status;
}

i32 Io_FlushFile(Ob_HandleTable *table, Ob_Handle fileHandle)
{
    Io_File *file   = nullptr;
    i32      status = IoResolveFile(table, fileHandle, &file);
    if (status != kIoSuccess)
        return status;

    Io_Device *device    = file->Device;
    Io_Device *topDevice = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(topDevice->StackSize);
    if (!irp)
    {
        Ob_DereferenceObject(file);
        return kIoInsufficientResources;
    }

    Io_StackLocation *sl = Io_GetNextStackLocation(irp);
    sl->MajorFunction    = IRP_MJ_FLUSH;
    sl->FileObject       = file;

    irp->Flags |= IRP_FLAG_SYNCHRONOUS;

    status = Io_CallDriver(topDevice, irp);
    if (status == kIoPending)
    {
        Ds_WaitForObject(&irp->CompletionEvent.Header, DS_TIMEOUT_INFINITE);
        status = irp->Status;
    }

    Io_FreeIrp(irp);
    Ob_DereferenceObject(file);
    return status;
}

i32 Io_QueryFileInformation(Ob_HandleTable *table, Ob_Handle fileHandle, u32 infoType, void *buffer, usize *inOutSize)
{
    Io_File *file   = nullptr;
    i32      status = IoResolveFile(table, fileHandle, &file);
    if (status != kIoSuccess)
        return status;

    Io_Device *device    = file->Device;
    Io_Device *topDevice = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(topDevice->StackSize);
    if (!irp)
    {
        Ob_DereferenceObject(file);
        return kIoInsufficientResources;
    }

    Io_StackLocation *sl          = Io_GetNextStackLocation(irp);
    sl->MajorFunction             = IRP_MJ_QUERY_INFORMATION;
    sl->FileObject                = file;
    sl->Parameters.QueryInfo.Type = infoType;

    irp->UserBuffer = buffer;
    irp->Flags |= IRP_FLAG_SYNCHRONOUS;

    if (device->Flags & IO_DEVICE_BUFFERED_IO)
    {
        irp->SystemBuffer = Ex_Allocate(*inOutSize, IO_TAG_IRP);
        if (!irp->SystemBuffer)
        {
            Io_FreeIrp(irp);
            Ob_DereferenceObject(file);
            return kIoInsufficientResources;
        }

        Core_ZeroMemory(irp->SystemBuffer, *inOutSize);
        irp->Flags |= IRP_FLAG_BUFFERED;
    }

    status = Io_CallDriver(topDevice, irp);
    if (status == kIoPending)
    {
        Ds_WaitForObject(&irp->CompletionEvent.Header, DS_TIMEOUT_INFINITE);
        status = irp->Status;
    }

    *inOutSize = irp->Information;

    Io_FreeIrp(irp);
    Ob_DereferenceObject(file);
    return status;
}

i32 Io_SetFileInformation(Ob_HandleTable *table, Ob_Handle fileHandle, u32 infoType, const void *buffer, usize size)
{
    Io_File *file   = nullptr;
    i32      status = IoResolveFile(table, fileHandle, &file);
    if (status != kIoSuccess)
        return status;

    Io_Device *device    = file->Device;
    Io_Device *topDevice = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(topDevice->StackSize);
    if (!irp)
    {
        Ob_DereferenceObject(file);
        return kIoInsufficientResources;
    }

    Io_StackLocation *sl          = Io_GetNextStackLocation(irp);
    sl->MajorFunction             = IRP_MJ_SET_INFORMATION;
    sl->FileObject                = file;
    sl->Parameters.QueryInfo.Type = infoType;

    irp->UserBuffer = (void *)buffer;
    irp->Flags |= IRP_FLAG_SYNCHRONOUS;

    if (device->Flags & IO_DEVICE_BUFFERED_IO)
    {
        irp->SystemBuffer = Ex_Allocate(size, IO_TAG_IRP);
        if (!irp->SystemBuffer)
        {
            Io_FreeIrp(irp);
            Ob_DereferenceObject(file);
            return kIoInsufficientResources;
        }

        Core_CopyMemory(irp->SystemBuffer, buffer, size);
        irp->Flags |= IRP_FLAG_BUFFERED;
    }

    status = Io_CallDriver(topDevice, irp);
    if (status == kIoPending)
    {
        Ds_WaitForObject(&irp->CompletionEvent.Header, DS_TIMEOUT_INFINITE);
        status = irp->Status;
    }

    Io_FreeIrp(irp);
    Ob_DereferenceObject(file);
    return status;
}

i32 Io_CloseFile(Ob_HandleTable *table, Ob_Handle fileHandle)
{
    Io_File *file   = nullptr;
    i32      status = IoResolveFile(table, fileHandle, &file);
    if (status != kIoSuccess)
        return status;

    Io_Device *device    = file->Device;
    Io_Device *topDevice = Io_GetTopDevice(device);

    Io_Irp *irp = Io_AllocateIrp(topDevice->StackSize);
    if (irp)
    {
        Io_StackLocation *sl = Io_GetNextStackLocation(irp);
        sl->MajorFunction    = IRP_MJ_CLEANUP;
        sl->FileObject       = file;

        irp->Flags |= IRP_FLAG_SYNCHRONOUS;

        i32 cleanupStatus = Io_CallDriver(topDevice, irp);
        if (cleanupStatus == kIoPending)
            Ds_WaitForObject(&irp->CompletionEvent.Header, DS_TIMEOUT_INFINITE);

        Io_FreeIrp(irp);
    }

    Ob_DereferenceObject(file);
    Ob_CloseHandle(table, fileHandle);

    return kIoSuccess;
}
