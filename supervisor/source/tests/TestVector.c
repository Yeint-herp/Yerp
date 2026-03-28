#ifdef CI_BUILD

#include <dsa/Vector.h>
#include <tests/CiTest.h>

CI_TEST("vector: push_pop", VectorPushPop)
{
    vector_of(int) v = nullptr;

    Dsa_VectorPush(v, 10);
    Dsa_VectorPush(v, 20);
    Dsa_VectorPush(v, 30);

    if (Dsa_VectorCount(v) != 3)
        return false;

    if (v[0] != 10 || v[1] != 20 || v[2] != 30)
        return false;

    int last = Dsa_VectorPop(v);
    if (last != 30 || Dsa_VectorCount(v) != 2)
        return false;

    Dsa_VectorFree(v);
    return true;
}

CI_TEST("vector: grow", VectorGrow)
{
    vector_of(u32) v = nullptr;

    for (u32 i = 0; i < 100; i++)
        Dsa_VectorPush(v, i * 3);

    if (Dsa_VectorCount(v) != 100)
        return false;

    for (u32 i = 0; i < 100; i++)
        if (v[i] != i * 3)
            return false;

    if (Dsa_VectorCapacity(v) < 100)
        return false;

    Dsa_VectorFree(v);
    return true;
}

CI_TEST("vector: clear", VectorClear)
{
    vector_of(int) v = nullptr;

    Dsa_VectorPush(v, 1);
    Dsa_VectorPush(v, 2);
    Dsa_VectorClear(v);

    if (Dsa_VectorCount(v) != 0)
        return false;

    if (Dsa_VectorCapacity(v) < 2)
        return false;

    Dsa_VectorPush(v, 42);
    if (v[0] != 42 || Dsa_VectorCount(v) != 1)
        return false;

    Dsa_VectorFree(v);
    return true;
}

CI_TEST("vector: tag", VectorTag)
{
    vector_of(u64) v = nullptr;

    Dsa_VectorPush(v, 0xAA, EX_TAG_OBJECT);
    if (Dsa_VectorHdr(v)->Tag != EX_TAG_OBJECT)
        return false;

    Dsa_VectorPush(v, 0xBB);
    if (Dsa_VectorHdr(v)->Tag != EX_TAG_OBJECT)
        return false;

    Dsa_VectorFree(v);
    return true;
}

CI_TEST("vector: foreach", VectorForEach)
{
    vector_of(int) v = nullptr;

    Dsa_VectorPush(v, 1);
    Dsa_VectorPush(v, 2);
    Dsa_VectorPush(v, 3);

    int sum = 0;
    Dsa_VectorForEach(v, it) sum += *it;

    if (sum != 6)
        return false;

    Dsa_VectorFree(v);
    return true;
}

CI_TEST("vector: last", VectorLast)
{
    vector_of(int) v = nullptr;

    Dsa_VectorPush(v, 5);
    Dsa_VectorPush(v, 10);

    if (Dsa_VectorLast(v) != 10)
        return false;

    Dsa_VectorPop(v);
    if (Dsa_VectorLast(v) != 5)
        return false;

    Dsa_VectorFree(v);
    return true;
}

CI_TEST("vector: empty_ops", VectorEmptyOps)
{
    vector_of(int) v = nullptr;

    if (Dsa_VectorCount(v) != 0)
        return false;
    if (Dsa_VectorCapacity(v) != 0)
        return false;

    Dsa_VectorClear(v);
    Dsa_VectorFree(v);

    return true;
}

CI_TEST("vector: struct", VectorStruct)
{
    typedef struct
    {
        u32 x, y;
    } Point;

    vector_of(Point) v = nullptr;

    Dsa_VectorPush(v, ((Point){.x = 1, .y = 2}));
    Dsa_VectorPush(v, ((Point){.x = 3, .y = 4}));

    if (v[0].x != 1 || v[0].y != 2)
        return false;
    if (v[1].x != 3 || v[1].y != 4)
        return false;

    Point p = Dsa_VectorPop(v);
    if (p.x != 3 || p.y != 4)
        return false;

    Dsa_VectorFree(v);
    return true;
}

#endif /* CI_BUILD */
