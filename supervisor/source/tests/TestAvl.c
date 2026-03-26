#ifdef CI_BUILD

#include <dsa/Avl.h>
#include <tests/CiTest.h>

typedef struct
{
    i32 Key;
    u64 Value;

    Dsa_AvlNode TreeNode;
} AvlTestNode;

#define N(ptr) Dsa_AvlEntry(ptr, AvlTestNode, TreeNode)

static i32 NodeCmp(Dsa_AvlNode *a, Dsa_AvlNode *b)
{
    i32 ka = N(a)->Key;
    i32 kb = N(b)->Key;
    if (ka < kb)
        return -1;

    if (ka > kb)
        return 1;

    return 0;
}

static i32 KeyCmp(void *key, Dsa_AvlNode *node)
{
    i32 k = (i32)(uptr)key;
    i32 n = N(node)->Key;
    if (k < n)
        return -1;

    if (k > n)
        return 1;

    return 0;
}

#define KEY(v) ((void *)(uptr)(v))

static i32 s_AvlNodeHeight(Dsa_AvlNode *n)
{
    if (!n)
        return 0;

    i32 l = s_AvlNodeHeight(n->Left);
    i32 r = s_AvlNodeHeight(n->Right);
    return 1 + (l > r ? l : r);
}

static bool s_ValidateAvl(Dsa_AvlNode *n, Dsa_AvlNode *expectedParent)
{
    if (!n)
        return true;

    if (n->Parent != expectedParent)
        return false;

    i32 lh = s_AvlNodeHeight(n->Left);
    i32 rh = s_AvlNodeHeight(n->Right);
    i32 bf = rh - lh;

    if (bf < -1 || bf > 1)
        return false;

    if (n->Balance != bf)
        return false;

    return s_ValidateAvl(n->Left, n) && s_ValidateAvl(n->Right, n);
}

static bool s_ValidateBst(Dsa_AvlNode *n, i32 min, i32 max)
{
    if (!n)
        return true;

    i32 key = N(n)->Key;
    if (key <= min || key >= max)
        return false;

    return s_ValidateBst(n->Left, min, key) && s_ValidateBst(n->Right, key, max);
}

static bool s_ValidateTree(Dsa_AvlTree *tree)
{
    if (!tree->Root)
        return tree->Count == 0;

    if (tree->Root->Parent != nullptr)
        return false;

    return s_ValidateAvl(tree->Root, nullptr) && s_ValidateBst(tree->Root, -2147483648, 2147483647);
}

static usize s_CountNodes(Dsa_AvlTree *tree)
{
    usize count = 0;
    Dsa_AvlForEach(tree, it)
    {
        count++;
        (void)it;
    }
    return count;
}

CI_TEST("avl: empty tree", TestAvlEmpty)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    if (tree.Root != nullptr || tree.Count != 0)
        return false;

    if (Dsa_AvlFirst(&tree) != nullptr)
        return false;

    Dsa_AvlNode *found = Dsa_AvlFind(&tree, KEY(42), KeyCmp);
    if (found != nullptr)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: single insert and find", TestAvlSingleInsert)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode a  = {.Key = 10, .Value = 0xAA};
    bool        ok = Dsa_AvlInsert(&tree, &a.TreeNode, NodeCmp);

    if (!ok || tree.Count != 1)
        return false;

    Dsa_AvlNode *found = Dsa_AvlFind(&tree, KEY(10), KeyCmp);
    if (!found || N(found)->Key != 10 || N(found)->Value != 0xAA)
        return false;

    if (Dsa_AvlFind(&tree, KEY(99), KeyCmp) != nullptr)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: duplicate rejected", TestAvlDuplicate)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode a = {.Key = 5};
    AvlTestNode b = {.Key = 5};

    Dsa_AvlInsert(&tree, &a.TreeNode, NodeCmp);
    bool ok = Dsa_AvlInsert(&tree, &b.TreeNode, NodeCmp);

    if (ok)
        return false;

    if (tree.Count != 1)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: ascending insert order", TestAvlAscending)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[16];
    for (i32 i = 0; i < 16; i++)
    {
        nodes[i].Key   = i;
        nodes[i].Value = (u64)i * 100;
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    if (tree.Count != 16)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    for (i32 i = 0; i < 16; i++)
    {
        Dsa_AvlNode *f = Dsa_AvlFind(&tree, KEY(i), KeyCmp);
        if (!f || N(f)->Key != i)
            return false;
    }

    return true;
}

CI_TEST("avl: descending insert order", TestAvlDescending)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[16];
    for (i32 i = 15; i >= 0; i--)
    {
        nodes[i].Key   = i;
        nodes[i].Value = (u64)i;
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    if (tree.Count != 16)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: zigzag insert order", TestAvlZigzag)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    i32         keys[] = {50, 10, 90, 5, 80, 15, 70, 20, 60, 25};
    AvlTestNode nodes[10];

    for (i32 i = 0; i < 10; i++)
    {
        nodes[i].Key = keys[i];
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);

        if (!s_ValidateTree(&tree))
            return false;
    }

    if (tree.Count != 10)
        return false;

    return true;
}

CI_TEST("avl: in-order traversal", TestAvlTraversal)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    i32         keys[] = {30, 10, 50, 5, 20, 40, 60};
    AvlTestNode nodes[7];

    for (i32 i = 0; i < 7; i++)
    {
        nodes[i].Key = keys[i];
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    i32   prev  = -2147483647;
    usize count = 0;
    Dsa_AvlForEach(&tree, it)
    {
        i32 k = N(it)->Key;
        if (k <= prev)
            return false;

        prev = k;
        count++;
    }

    if (count != 7)
        return false;

    return true;
}

CI_TEST("avl: prev and next", TestAvlPrevNext)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[5];
    i32         keys[] = {10, 20, 30, 40, 50};

    for (i32 i = 0; i < 5; i++)
    {
        nodes[i].Key = keys[i];
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    Dsa_AvlNode *mid = Dsa_AvlFind(&tree, KEY(30), KeyCmp);
    if (!mid)
        return false;

    Dsa_AvlNode *n = Dsa_AvlNext(mid);

    if (!n || N(n)->Key != 40)
        return false;

    Dsa_AvlNode *first = Dsa_AvlFirst(&tree);
    if (!first || N(first)->Key != 10)
        return false;

    Dsa_AvlNode *last = Dsa_AvlFind(&tree, KEY(50), KeyCmp);
    if (!last || Dsa_AvlNext(last) != nullptr)
        return false;

    return true;
}

CI_TEST("avl: remove leaf", TestAvlRemoveLeaf)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[5];
    i32         keys[] = {20, 10, 30, 5, 25};

    for (i32 i = 0; i < 5; i++)
    {
        nodes[i].Key = keys[i];
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    Dsa_AvlNode *removed = Dsa_AvlRemove(&tree, KEY(5), KeyCmp);
    if (!removed || N(removed)->Key != 5)
        return false;

    if (tree.Count != 4)
        return false;

    if (Dsa_AvlFind(&tree, KEY(5), KeyCmp) != nullptr)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: remove node with one child", TestAvlRemoveOneChild)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[4];
    i32         keys[] = {20, 10, 30, 25};

    for (i32 i = 0; i < 4; i++)
    {
        nodes[i].Key = keys[i];
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    Dsa_AvlNode *removed = Dsa_AvlRemove(&tree, KEY(30), KeyCmp);
    if (!removed || N(removed)->Key != 30)
        return false;

    if (tree.Count != 3)
        return false;

    if (!Dsa_AvlFind(&tree, KEY(25), KeyCmp))
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: remove node with two children", TestAvlRemoveTwoChildren)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[7];
    i32         keys[] = {20, 10, 30, 5, 15, 25, 35};

    for (i32 i = 0; i < 7; i++)
    {
        nodes[i].Key = keys[i];
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    Dsa_AvlNode *removed = Dsa_AvlRemove(&tree, KEY(20), KeyCmp);
    if (!removed || N(removed)->Key != 20)
        return false;

    if (tree.Count != 6)
        return false;

    i32 remaining[] = {5, 10, 15, 25, 30, 35};
    for (i32 i = 0; i < 6; i++)
        if (!Dsa_AvlFind(&tree, KEY(remaining[i]), KeyCmp))
            return false;

    if (!s_ValidateTree(&tree))
        return false;

    i32 prev = -2147483647;
    Dsa_AvlForEach(&tree, it)
    {
        i32 k = N(it)->Key;
        if (k <= prev)
            return false;

        prev = k;
    }

    return true;
}

CI_TEST("avl: remove root", TestAvlRemoveRoot)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode a = {.Key = 42};
    Dsa_AvlInsert(&tree, &a.TreeNode, NodeCmp);

    Dsa_AvlNode *removed = Dsa_AvlRemove(&tree, KEY(42), KeyCmp);
    if (!removed || N(removed)->Key != 42)
        return false;

    if (tree.Count != 0 || tree.Root != nullptr)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: remove nonexistent", TestAvlRemoveNotFound)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode a = {.Key = 10};
    Dsa_AvlInsert(&tree, &a.TreeNode, NodeCmp);

    Dsa_AvlNode *removed = Dsa_AvlRemove(&tree, KEY(99), KeyCmp);
    if (removed != nullptr)
        return false;

    if (tree.Count != 1)
        return false;

    return true;
}

CI_TEST("avl: remove all ascending", TestAvlRemoveAllAscending)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[16];
    for (i32 i = 0; i < 16; i++)
    {
        nodes[i].Key = i * 3;
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    for (i32 i = 0; i < 16; i++)
    {
        Dsa_AvlNode *r = Dsa_AvlRemove(&tree, KEY(i * 3), KeyCmp);
        if (!r)
            return false;

        if (!s_ValidateTree(&tree))
            return false;

        if (s_CountNodes(&tree) != tree.Count)
            return false;
    }

    if (tree.Count != 0 || tree.Root != nullptr)
        return false;

    return true;
}

CI_TEST("avl: remove all descending", TestAvlRemoveAllDescending)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[16];
    for (i32 i = 0; i < 16; i++)
    {
        nodes[i].Key = i * 3;
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    for (i32 i = 15; i >= 0; i--)
    {
        Dsa_AvlNode *r = Dsa_AvlRemove(&tree, KEY(i * 3), KeyCmp);
        if (!r)
            return false;

        if (!s_ValidateTree(&tree))
            return false;
    }

    if (tree.Count != 0)
        return false;

    return true;
}

CI_TEST("avl: remove alternating", TestAvlRemoveAlternating)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[20];
    for (i32 i = 0; i < 20; i++)
    {
        nodes[i].Key = i;
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    for (i32 i = 0; i < 20; i += 2)
    {
        Dsa_AvlRemove(&tree, KEY(i), KeyCmp);
        if (!s_ValidateTree(&tree))
            return false;
    }

    if (tree.Count != 10)
        return false;

    for (i32 i = 0; i < 20; i++)
    {
        Dsa_AvlNode *f = Dsa_AvlFind(&tree, KEY(i), KeyCmp);
        if (i % 2 == 0 && f != nullptr)
            return false;
        if (i % 2 == 1 && (!f || N(f)->Key != i))
            return false;
    }

    return true;
}

CI_TEST("avl: insert-remove-reinsert", TestAvlReinsert)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[8];
    for (i32 i = 0; i < 8; i++)
    {
        nodes[i].Key = i * 10;
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    Dsa_AvlRemove(&tree, KEY(30), KeyCmp);
    Dsa_AvlRemove(&tree, KEY(50), KeyCmp);

    if (tree.Count != 6)
        return false;

    nodes[3].Key   = 30;
    nodes[3].Value = 0xFF;
    nodes[5].Key   = 50;
    nodes[5].Value = 0xEE;
    Dsa_AvlInsert(&tree, &nodes[3].TreeNode, NodeCmp);
    Dsa_AvlInsert(&tree, &nodes[5].TreeNode, NodeCmp);

    if (tree.Count != 8)
        return false;

    Dsa_AvlNode *f3 = Dsa_AvlFind(&tree, KEY(30), KeyCmp);
    Dsa_AvlNode *f5 = Dsa_AvlFind(&tree, KEY(50), KeyCmp);
    if (!f3 || N(f3)->Value != 0xFF)
        return false;
    if (!f5 || N(f5)->Value != 0xEE)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

CI_TEST("avl: stress sequential", TestAvlStressSequential)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[128];
    for (i32 i = 0; i < 128; i++)
    {
        nodes[i].Key   = i;
        nodes[i].Value = (u64)i * (u64)i;
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    if (tree.Count != 128)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    i32 height = s_AvlNodeHeight(tree.Root);
    if (height > 10)
        return false;

    for (i32 i = 0; i < 128; i++)
    {
        Dsa_AvlNode *f = Dsa_AvlFind(&tree, KEY(i), KeyCmp);
        if (!f || N(f)->Value != (u64)i * (u64)i)
            return false;
    }

    for (i32 i = 0; i < 64; i++)
        Dsa_AvlRemove(&tree, KEY(i), KeyCmp);

    if (tree.Count != 64)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    for (i32 i = 64; i < 128; i++)
    {
        Dsa_AvlNode *f = Dsa_AvlFind(&tree, KEY(i), KeyCmp);
        if (!f || N(f)->Key != i)
            return false;
    }

    return true;
}

CI_TEST("avl: entry macro recovery", TestAvlEntryMacro)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode a = {.Key = 7, .Value = 0xDEAD};
    AvlTestNode b = {.Key = 3, .Value = 0xBEEF};
    AvlTestNode c = {.Key = 11, .Value = 0xCAFE};

    Dsa_AvlInsert(&tree, &a.TreeNode, NodeCmp);
    Dsa_AvlInsert(&tree, &b.TreeNode, NodeCmp);
    Dsa_AvlInsert(&tree, &c.TreeNode, NodeCmp);

    Dsa_AvlNode *found = Dsa_AvlFind(&tree, KEY(3), KeyCmp);
    AvlTestNode *entry = Dsa_AvlEntry(found, AvlTestNode, TreeNode);

    if (entry != &b)
        return false;

    if (entry->Value != 0xBEEF)
        return false;

    Dsa_AvlNode *first = Dsa_AvlFirst(&tree);
    if (Dsa_AvlEntry(first, AvlTestNode, TreeNode) != &b)
        return false;

    return true;
}

CI_TEST("avl: traversal count matches tree count", TestAvlTraversalCount)
{
    Dsa_AvlTree tree;
    Dsa_AvlTreeInit(&tree);

    AvlTestNode nodes[32];
    i32         keys[] = {15, 3,  27, 8, 22, 1,  19, 31, 6,  12, 25, 4,  17, 29, 10, 2,
                          20, 14, 28, 7, 23, 11, 30, 5,  18, 26, 9,  13, 24, 16, 21, 0};

    for (i32 i = 0; i < 32; i++)
    {
        nodes[i].Key = keys[i];
        Dsa_AvlInsert(&tree, &nodes[i].TreeNode, NodeCmp);
    }

    usize count = s_CountNodes(&tree);
    if (count != tree.Count || count != 32)
        return false;

    for (i32 i = 0; i < 10; i++)
        Dsa_AvlRemove(&tree, KEY(keys[i]), KeyCmp);

    count = s_CountNodes(&tree);
    if (count != tree.Count || count != 22)
        return false;

    if (!s_ValidateTree(&tree))
        return false;

    return true;
}

#endif /* CI_BUILD */
