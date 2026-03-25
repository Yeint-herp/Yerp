#ifdef CI_BUILD

#define DBG_MODULE "CiTest"

#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <tests/CiTest.h>

extern const Ci_TestCase __start_ci_tests[];
extern const Ci_TestCase __stop_ci_tests[];

u8 CiTest_Entry(void)
{
    const Ci_TestCase *it  = __start_ci_tests;
    const Ci_TestCase *end = __stop_ci_tests;

    usize total  = end - it;
    usize passed = 0;
    usize failed = 0;

    Log(INFO, "running %zu test(s)", total);

    for (; it < end; it++)
    {
        bool ok = it->func();
        if (ok)
        {
            Log(INFO, "  PASS: %s", it->name);
            passed++;
        }
        else
        {
            Log(ERROR, "  FAIL: %s", it->name);
            failed++;
        }
    }

    Log(INFO, "results: %zu passed, %zu failed, %zu total", passed, failed, total);

    if (failed > 0)
        return 0x1;
    else
        return 0x0;
}

#endif /* CI_BUILD */
