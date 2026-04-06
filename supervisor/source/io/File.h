#ifndef SUPERVISOR_IO_FILE_H
#define SUPERVISOR_IO_FILE_H

#include <core/Spinlock.h>
#include <dispatcher/Dispatcher.h>
#include <executive/Acl.h>
#include <executive/Object.h>

struct Io_Device;

#define IO_FILE_READ       (1u << 0)
#define IO_FILE_WRITE      (1u << 1)
#define IO_FILE_APPEND     (1u << 2)
#define IO_FILE_EXECUTE    (1u << 3)
#define IO_FILE_ALL_ACCESS (IO_FILE_READ | IO_FILE_WRITE | IO_FILE_APPEND | IO_FILE_EXECUTE)

#define IO_FILE_SYNCHRONOUS     (1u << 0)
#define IO_FILE_DELETE_ON_CLOSE (1u << 1)

typedef struct Io_File
{
    struct Io_Device *Device;

    u64 CurrentOffset;
    u32 Flags;
    u32 ShareAccess;

    Ds_Event      Event;
    Core_Spinlock Lock;

    void *FsContext;
    void *FsContext2;
} Io_File;

Ob_Type *Io_GetFileType(void);

i32 Io_CreateFile(Ob_HandleTable *table, Acl_Token *token, const char *path, u32 desiredAccess, u32 shareAccess,
                  u32 createFlags, Ob_Handle *outHandle);

i32 Io_ReadFile(Ob_HandleTable *table, Ob_Handle fileHandle, void *buffer, usize length, u64 *offset, usize *bytesRead);

i32 Io_WriteFile(Ob_HandleTable *table, Ob_Handle fileHandle, const void *buffer, usize length, u64 *offset,
                 usize *bytesWritten);

i32 Io_DeviceIoControl(Ob_HandleTable *table, Ob_Handle fileHandle, u32 controlCode, const void *inBuffer,
                       usize inLength, void *outBuffer, usize outLength, usize *bytesReturned);

i32 Io_FlushFile(Ob_HandleTable *table, Ob_Handle fileHandle);

i32 Io_QueryFileInformation(Ob_HandleTable *table, Ob_Handle fileHandle, u32 infoType, void *buffer, usize *inOutSize);

i32 Io_SetFileInformation(Ob_HandleTable *table, Ob_Handle fileHandle, u32 infoType, const void *buffer, usize size);

i32 Io_CloseFile(Ob_HandleTable *table, Ob_Handle fileHandle);

#endif /* SUPERVISOR_IO_FILE_H */
