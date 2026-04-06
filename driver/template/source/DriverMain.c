#define DBG_MODULE "TemplateDriver"

#include <debug/DbgPrint.h>
#include <io/Driver.h>

void DriverUnload(Io_Driver *)
{
    Log(INFO, "Unloaded");
}

i32 DriverEntry(Io_Driver *Driver)
{
    Driver->DriverUnload = DriverUnload;

    Log(INFO, "Loaded");
    return 0;
}
