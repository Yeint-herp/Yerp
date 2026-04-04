#define DBG_MODULE "Executive"

#include <acpi/Acpi.h>
#include <arch/CoreLocal.h>
#include <arch/CpuCap.h>
#include <arch/Interrupts.h>
#include <arch/Irql.h>
#include <arch/MmArch.h>
#include <arch/Rng.h>
#include <arch/x86_64/RegisterFrame.h>
#include <arch/x86_64/Spcr.h>
#include <boot/Limine.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <dispatcher/Vm.h>
#include <executive/Dpc.h>
#include <executive/Init.h>
#include <executive/Object.h>
#include <executive/Pool.h>
#include <executive/Timer.h>
#include <mm/Early.h>
#include <mm/MemMap.h>
#include <mm/PfnDb.h>
#include <mm/Vas.h>
#include <mm/ZeroPage.h>

void Core_StackGuardInit(void);

void Ex_InitializeLate(void *param)
{
    (void)param;
    Log(INFO, "reached late initialization!");

    Mm_ZeroPageInit();

    Ds_ThreadExit(0);
}

[[gnu::no_stack_protector]]
void Ex_InitializeEarly()
{
    Dbg_RegisterSinker(g_RingBufSinker);
    Log(TRACE, "early ring buffer sinker ready");

    if (kArch == x86_64)
        Dbg_RegisterSinker(g_e9Sinker);

    Arch_CpuCapInit();
    Log(TRACE, "queried cpu capabilities");

    Arch_RngInit();
    const Arch_RngSource rngSource = Arch_RngGetSource();
    if (rngSource == RNG_SRC_NONE)
        Log(ERROR, "no hrng available");
    else
        Log(TRACE, "initialized hrng using %s",
            (rngSource == RNG_SRC_RDSEED)   ? "hardware RdSeed"
            : (rngSource == RNG_SRC_RDRAND) ? "hardware RndRand"
                                            : "hardware TSC");

    if (!LIMINE_BASE_REVISION_SUPPORTED(Boot_LimineBaseRevision) || !Boot_LimineMemmapReq.response ||
        !Boot_LimineHhdmReq.response)
        Panic("critical bootloader requests not fulfilled");

    Core_StackGuardInit();

    Mm_EarlyInit(Boot_LimineMemmapReq.response, Boot_LimineHhdmReq.response->offset);
    Arch_MmInit();

    Log(INFO, "hhdm handed of at %#llx", Mm_GetHhdmBase());
    Mm_DumpMemMap(Mm_GetSupervisorMemMap());

    bool hasMultiprocessor = Core_SpcbAllocateAll(Boot_LimineSmpReq.response);
    if (!hasMultiprocessor)
        Log(INFO, "multiprocessor capabilities not detected");

    Mm_PfnDbInit();
    Mm_SetPfnReady();

    Ex_PoolInit();
    Mm_SupervisorVasInit();

    Acpi_EarlyInit();
    Interrupt_ControllerInit();

    Core_SpcbBootAll(Boot_LimineSmpReq.response);

    Ob_Init();
    Dpc_SystemInit();

    Ex_TimerSystemInit();

    Log(INFO, "early initialization done");

    Ds_SystemInit();
    Ds_SchedulerSystemInit();

    Ds_Thread *initThread = nullptr;
    Ds_CreateThread(Ds_GetSystemVm(), Ex_InitializeLate, nullptr, DS_PRIORITY_NORMAL, &initThread);
    Ds_ReadyThread(initThread);

    Core_SpcbReleaseAps();

    Ds_EnterDispatcher();
    unreachable();
}

void Ex_DefaultInterruptHandler(Arch_RegisterFrame *frame)
{
    Panic_Fault(frame, "interrupt invoked before handler ready");
}
