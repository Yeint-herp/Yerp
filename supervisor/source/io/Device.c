#define DBG_MODULE "IoDevice"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <executive/Pool.h>
#include <io/Device.h>
#include <io/Driver.h>
#include <io/Io.h>

void Io_DeviceQueueInit(Io_DeviceQueue *queue)
{
    Dsa_ListInit(&queue->Head);
    Core_SpinlockInit(&queue->Lock);
    queue->Busy = false;
}

i32 Io_CreateDevice(Io_Driver *driver, usize extensionSize, const char *name, u32 deviceType, u32 flags,
                    Io_Device **outDevice)
{
    if (!driver || !outDevice)
        return kIoInvalidParameter;

    Io_Device *dev    = nullptr;
    i32        status = Ob_CreateObject(Io_GetDeviceType(), 0, (void **)&dev);
    if (status != kObSuccess)
        return kIoInsufficientResources;

    Core_ZeroMemory(dev, sizeof *dev);

    dev->Driver               = driver;
    dev->DeviceType           = deviceType;
    dev->Flags                = flags | IO_DEVICE_INITIALIZING;
    dev->StackSize            = 1;
    dev->AlignmentRequirement = 0;
    dev->ReferenceCount       = 1;

    Dsa_ListInit(&dev->DriverLink);
    Core_SpinlockInit(&dev->Lock);
    Io_DeviceQueueInit(&dev->Queue);

    if (extensionSize > 0)
    {
        dev->DeviceExtension = Ex_Allocate(extensionSize, IO_TAG_DEVICE);
        if (!dev->DeviceExtension)
        {
            Ob_DereferenceObject(dev);
            return kIoInsufficientResources;
        }

        Core_ZeroMemory(dev->DeviceExtension, extensionSize);
        dev->DeviceExtensionSize = extensionSize;
    }

    if (name)
    {
        usize len = Core_StringLength(name);

        dev->DeviceName = Ex_Allocate(len + 1, IO_TAG_DEVICE);
        if (!dev->DeviceName)
        {
            if (dev->DeviceExtension)
                Ex_Free(dev->DeviceExtension);

            Ob_DereferenceObject(dev);
            return kIoInsufficientResources;
        }

        Core_CopyString(dev->DeviceName, name);

        status = Ob_InsertObjectByPath(name, dev);
        if (status != kObSuccess)
        {
            Ex_Free(dev->DeviceName);
            if (dev->DeviceExtension)
                Ex_Free(dev->DeviceExtension);

            Ob_DereferenceObject(dev);
            return kIoInsufficientResources;
        }
    }

    Arch_IrqFlags irqFlags = Core_SpinlockAcquireIrq(&driver->DeviceListLock);
    Dsa_ListInsertTail(&driver->DeviceListHead, &dev->DriverLink);
    driver->DeviceCount++;
    Core_SpinlockReleaseIrq(&driver->DeviceListLock, irqFlags);

    dev->Flags &= ~IO_DEVICE_INITIALIZING;

    Log(INFO, "created device '%s' type %u for driver '%s'", name ? name : "(unnamed)", deviceType, driver->Name);

    *outDevice = dev;
    return kIoSuccess;
}

void Io_DeleteDevice(Io_Device *device)
{
    if (!device)
        return;

    Io_Driver *driver = device->Driver;

    Arch_IrqFlags irqFlags = Core_SpinlockAcquireIrq(&driver->DeviceListLock);
    Dsa_ListRemoveHead(&device->DriverLink);
    driver->DeviceCount--;
    Core_SpinlockReleaseIrq(&driver->DeviceListLock, irqFlags);

    if (device->AttachedDevice)
        Io_DetachDevice(device);

    Ob_DereferenceObject(device);
}

void *Io_GetDeviceExtension(Io_Device *device)
{
    return device->DeviceExtension;
}

Io_Device *Io_AttachDevice(Io_Device *sourceDevice, Io_Device *targetDevice)
{
    Io_Device *topDevice = Io_GetTopDevice(targetDevice);

    Arch_IrqFlags irqFlags = Core_SpinlockAcquireIrq(&topDevice->Lock);

    topDevice->AttachedDevice = sourceDevice;
    sourceDevice->LowerDevice = topDevice;
    sourceDevice->StackSize   = topDevice->StackSize + 1;

    Core_SpinlockReleaseIrq(&topDevice->Lock, irqFlags);

    Log(TRACE, "attached device onto stack, new depth %u", sourceDevice->StackSize);

    return topDevice;
}

void Io_DetachDevice(Io_Device *targetDevice)
{
    Arch_IrqFlags irqFlags = Core_SpinlockAcquireIrq(&targetDevice->Lock);

    Io_Device *attached = targetDevice->AttachedDevice;
    if (attached)
    {
        attached->LowerDevice        = nullptr;
        targetDevice->AttachedDevice = nullptr;
    }

    Core_SpinlockReleaseIrq(&targetDevice->Lock, irqFlags);
}

Io_Device *Io_GetTopDevice(Io_Device *device)
{
    while (device->AttachedDevice)
        device = device->AttachedDevice;

    return device;
}

void Io_StartPacket(Io_Device *device, Io_Irp *irp)
{
    Arch_IrqFlags irqFlags = Core_SpinlockAcquireIrq(&device->Queue.Lock);

    if (!device->Queue.Busy)
    {
        device->Queue.Busy = true;
        Core_SpinlockReleaseIrq(&device->Queue.Lock, irqFlags);

        Io_CallDriver(device, irp);
    }
    else
    {
        Dsa_ListInsertTail(&device->Queue.Head, &irp->ListEntry);
        Core_SpinlockReleaseIrq(&device->Queue.Lock, irqFlags);
    }
}

void Io_StartNextPacket(Io_Device *device)
{
    Arch_IrqFlags irqFlags = Core_SpinlockAcquireIrq(&device->Queue.Lock);

    if (Dsa_ListIsEmpty(&device->Queue.Head))
    {
        device->Queue.Busy = false;
        Core_SpinlockReleaseIrq(&device->Queue.Lock, irqFlags);
        return;
    }

    Dsa_ListEntry *entry = Dsa_ListRemoveHead(&device->Queue.Head);
    Core_SpinlockReleaseIrq(&device->Queue.Lock, irqFlags);

    Io_Irp *nextIrp = container_of(entry, Io_Irp, ListEntry);
    Io_CallDriver(device, nextIrp);
}

i32 Io_CallDriver(Io_Device *device, Io_Irp *irp)
{
    irp->CurrentLocation--;
    Io_StackLocation *sl = Io_GetCurrentStackLocation(irp);
    sl->Device           = device;

    Io_Driver *driver = device->Driver;
    return driver->DispatchTable[sl->MajorFunction](device, irp);
}
