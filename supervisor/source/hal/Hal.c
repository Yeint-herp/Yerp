#include <debug/DbgPrint.h>
#include <hal/Hal.h>

void Hal_InitializeEarly()
{
    Dbg_RegisterSinker(g_RingBufSinker);
    Dbg_Log(TRACE, "hal", "early ring buffer sinker ready");

    if (kArch == x86_64)
        Dbg_RegisterSinker(g_e9Sinker);

    Dbg_Log(INFO, "hal", "early initialization done");
}
