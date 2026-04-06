#ifndef SUPERVISOR_IO_IRP_H
#define SUPERVISOR_IO_IRP_H

#include <core/Spinlock.h>
#include <dispatcher/Dispatcher.h>
#include <dsa/List.h>
#include <io/Io.h>

enum
{
    IRP_MJ_CREATE,
    IRP_MJ_CLOSE,
    IRP_MJ_READ,
    IRP_MJ_WRITE,
    IRP_MJ_DEVICE_CONTROL,
    IRP_MJ_INTERNAL_DEVICE_CONTROL,
    IRP_MJ_FLUSH,
    IRP_MJ_QUERY_INFORMATION,
    IRP_MJ_SET_INFORMATION,
    IRP_MJ_CLEANUP,
    IRP_MJ_SHUTDOWN,
    IRP_MJ_PNP,
    IRP_MJ_POWER,
    IRP_MJ_COUNT,
};

/// Minor for IRP_MJ_PNP.
enum
{
    IRP_MN_START_DEVICE,
    IRP_MN_STOP_DEVICE,
    IRP_MN_REMOVE_DEVICE,
    IRP_MN_QUERY_CAPABILITIES,
    IRP_MN_QUERY_DEVICE_RELATIONS,
};

/// Minor for IRP_MJ_POWER.
enum
{
    IRP_MN_SET_POWER,
    IRP_MN_QUERY_POWER,
};

struct Io_Device;
struct Io_File;
struct Io_Irp;

typedef void (*Io_CompletionRoutine)(struct Io_Device *device, struct Io_Irp *irp, void *context);

typedef struct Io_StackLocation
{
    u8 MajorFunction;
    u8 MinorFunction;
    u8 Flags;
    u8 Control;

    union
    {
        struct
        {
            usize Length;
            u64   Offset;
        } ReadWrite;

        struct
        {
            u32   ControlCode;
            usize InputLength;
            usize OutputLength;
            void *InputBuffer;
        } DeviceControl;

        struct
        {
            u32 Type;
        } QueryInfo;

        struct
        {
            u32 Type;
        } Pnp;

        struct
        {
            u32 State;
        } Power;
    } Parameters;

    struct Io_Device *Device;
    struct Io_File   *FileObject;

    Io_CompletionRoutine CompletionRoutine;
    void                *CompletionContext;
} Io_StackLocation;

#define IO_SL_INVOKE_ON_SUCCESS (1u << 0)
#define IO_SL_INVOKE_ON_ERROR   (1u << 1)
#define IO_SL_INVOKE_ON_CANCEL  (1u << 2)

#define IRP_FLAG_BUFFERED    (1u << 0)
#define IRP_FLAG_DIRECT      (1u << 1)
#define IRP_FLAG_PAGING_IO   (1u << 2)
#define IRP_FLAG_SYNCHRONOUS (1u << 3)

typedef void (*Io_CancelRoutine)(struct Io_Device *device, struct Io_Irp *irp);

typedef struct Io_Irp
{
    Dsa_ListEntry ListEntry;
    Ds_Event      CompletionEvent;

    i32   Status;
    usize Information;

    void *SystemBuffer;
    void *UserBuffer;

    u32 Flags;

    Io_CancelRoutine CancelRoutine;
    bool             Cancel;

    u32              StackCount;
    u32              CurrentLocation;
    Io_StackLocation Stack[] counted_by(StackCount);
} Io_Irp;

Io_Irp *Io_AllocateIrp(u32 stackSize);
void    Io_FreeIrp(Io_Irp *irp);

Io_StackLocation *Io_GetCurrentStackLocation(Io_Irp *irp);
Io_StackLocation *Io_GetNextStackLocation(Io_Irp *irp);

void Io_SetCompletionRoutine(Io_Irp *irp, Io_CompletionRoutine routine, void *context, u8 controlFlags);

void Io_SkipCurrentStackLocation(Io_Irp *irp);
void Io_CopyCurrentToNext(Io_Irp *irp);

void Io_CompleteRequest(Io_Irp *irp, i32 status, usize information);
void Io_MarkIrpPending(Io_Irp *irp);

void Io_CancelIrp(Io_Irp *irp);

#endif /* SUPERVISOR_IO_IRP_H */
