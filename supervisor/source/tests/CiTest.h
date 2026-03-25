#ifndef SUPERVISOR_TESTS_CI_TEST_H
#define SUPERVISOR_TESTS_CI_TEST_H

#include <debug/DbgPrint.h>
#include <debug/Panic.h>

typedef struct
{
    const char *name;
    bool (*func)(void);
} Ci_TestCase;

#define CI_TEST(Name, Func)                                                                                            \
    static bool Func(void);                                                                                            \
    [[gnu::used, gnu::section(".ci_tests")]]                                                                           \
    static const Ci_TestCase g_CiTest_##Func = {.name = (Name), .func = (Func)};                                       \
    static bool              Func(void)

u8 CiTest_Entry(void);

#endif /* SUPERVISOR_TESTS_CI_TEST_H */
