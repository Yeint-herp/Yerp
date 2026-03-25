#ifdef CI_BUILD

#include <arch/Rng.h>
#include <tests/CiTest.h>

CI_TEST("rng: source is available", RngSourceAvailable)
{
    Arch_RngSource src = Arch_RngGetSource();
    if (src == RNG_SRC_NONE)
    {
        Dbg_Print("FAIL: no RNG source available\n");
        return false;
    }

    Dbg_Print("RNG source: %d\n", src);
    return true;
}

CI_TEST("rng: u64 not stuck", RngU64NotStuck)
{
    u64 a = Arch_RngU64();
    u64 b = Arch_RngU64();
    u64 c = Arch_RngU64();

    if (a == b || b == c)
    {
        Dbg_Print("FAIL: consecutive RngU64 returned the same value: (%#llx, %#llx, %#llx)\n", a, b, c);
        return false;
    }

    return true;
}

CI_TEST("rng: fill works", RngFillWorks)
{
    u8 buf[64] = {};

    if (!Arch_RngFill(buf, sizeof buf))
    {
        Dbg_Print("FAIL: Arch_RngFill returned false\n");
        return false;
    }

    bool allZero = true;
    for (usize i = 0; i < sizeof buf; i++)
        if (buf[i] != 0)
        {
            allZero = false;
            break;
        }

    if (allZero)
    {
        Dbg_Print("FAIL: Arch_RngFill produced all zeroes\n");
        return false;
    }

    return true;
}

CI_TEST("rng: fill partial", RngFillPartial)
{
    u8 buf[11] = {};

    if (!Arch_RngFill(buf, sizeof buf))
    {
        Dbg_Print("FAIL: Arch_RngFill returned false for partial fill\n");
        return false;
    }

    bool allZero = true;
    for (usize i = 0; i < sizeof buf; i++)
        if (buf[i] != 0)
        {
            allZero = false;
            break;
        }

    if (allZero)
    {
        Dbg_Print("FAIL: partial Arch_RngFill produced all zeroes\n");
        return false;
    }

    return true;
}

CI_TEST("rng: fill zero length", RngFillZeroLen)
{
    u8 sentinel = 0xAB;

    if (!Arch_RngFill(&sentinel, 0))
    {
        Dbg_Print("FAIL: Arch_RngFill returned false for zero length\n");
        return false;
    }

    if (sentinel != 0xAB)
    {
        Dbg_Print("FAIL: Arch_RngFill modified memory on zero-length call\n");
        return false;
    }

    return true;
}

#endif /* CI_BUILD */
