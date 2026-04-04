#define DBG_MODULE "MmZeroPage"

#include <arch/CoreLocal.h>
#include <arch/MmArch.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <dispatcher/Dispatcher.h>
#include <dispatcher/Scheduler.h>
#include <dispatcher/Vm.h>
#include <mm/Early.h>
#include <mm/PfnDb.h>
#include <mm/ZeroPage.h>

#define ZERO_BATCH_SIZE 16

static Ds_Event s_FreePagesAvailable;

void Mm_ZeroPageNotify(void)
{
    Ds_EventSet(&s_FreePagesAvailable);
}

static void Mm_ZeroPageWorker(void *param)
{
    (void)param;

    Log(INFO, "zero-page scrubber online");

    for (;;)
    {
        for (;;)
        {
            bool batchEmpty = false;

            for (u32 i = 0; i < ZERO_BATCH_SIZE; i++)
            {
                uptr pfn = Mm_RemovePageFromList(&Mm_FreePageListHead);
                if (pfn == MM_PFN_NULL)
                {
                    batchEmpty = true;
                    break;
                }

                void *va = Mm_PhysToVirt(pfn << PAGE_SHIFT);
                Core_ZeroMemory(va, PAGE_SIZE);

                Mm_Pfn *entry = Mm_GetPfnEntry(pfn);

                Core_SpinlockAcquire(&entry->Lock);
                entry->e1.PageLocation = ZeroedPageList;
                Core_SpinlockRelease(&entry->Lock);

                Mm_InsertPageInList(&Mm_ZeroedPageListHead, pfn);
            }

            if (batchEmpty)
                break;

            Ds_Reschedule();
        }

        Ds_EventReset(&s_FreePagesAvailable);
        Ds_WaitForObject(&s_FreePagesAvailable.Header, DS_TIMEOUT_INFINITE);
    }
}

void Mm_ZeroPageInit(void)
{
    Ds_EventInit(&s_FreePagesAvailable, kDsObjectNotificationEvent, false);

    Ds_Thread *scrubber = nullptr;

    i32 status = Ds_CreateThread(Ds_GetSystemVm(), Mm_ZeroPageWorker, nullptr, DS_PRIORITY_LOWEST, &scrubber);

    if (status != 0)
    {
        Log(ERROR, "failed to create zero-page scrubber (%d)", status);
        return;
    }

    scrubber->IdealProcessor = Arch_GetCurrentSpcb()->ProcessorNumber;
    Ds_ReadyThread(scrubber);

    Log(INFO, "started zero-page scrubber");
}
