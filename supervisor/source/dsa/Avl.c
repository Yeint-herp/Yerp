#include <dsa/Avl.h>

static i32 s_AvlHeight(Dsa_AvlNode *n)
{
    if (!n)
        return 0;

    i32 l = s_AvlHeight(n->Left);
    i32 r = s_AvlHeight(n->Right);
    return 1 + (l > r ? l : r);
}

static void s_AvlUpdateBalance(Dsa_AvlNode *n)
{
    if (n)
        n->Balance = s_AvlHeight(n->Right) - s_AvlHeight(n->Left);
}

static Dsa_AvlNode *s_AvlRotateLeft(Dsa_AvlNode *n)
{
    Dsa_AvlNode *r = n->Right;
    n->Right       = r->Left;
    if (r->Left)
        r->Left->Parent = n;

    r->Parent = n->Parent;
    r->Left   = n;
    n->Parent = r;
    s_AvlUpdateBalance(n);
    s_AvlUpdateBalance(r);
    return r;
}

static Dsa_AvlNode *s_AvlRotateRight(Dsa_AvlNode *n)
{
    Dsa_AvlNode *l = n->Left;
    n->Left        = l->Right;
    if (l->Right)
        l->Right->Parent = n;

    l->Parent = n->Parent;
    l->Right  = n;
    n->Parent = l;
    s_AvlUpdateBalance(n);
    s_AvlUpdateBalance(l);
    return l;
}

static Dsa_AvlNode *s_AvlRebalance(Dsa_AvlNode *n)
{
    s_AvlUpdateBalance(n);

    if (n->Balance > 1)
    {
        if (n->Right && n->Right->Balance < 0)
            n->Right = s_AvlRotateRight(n->Right);

        return s_AvlRotateLeft(n);
    }
    if (n->Balance < -1)
    {
        if (n->Left && n->Left->Balance > 0)
            n->Left = s_AvlRotateLeft(n->Left);

        return s_AvlRotateRight(n);
    }
    return n;
}

Dsa_AvlNode *Dsa_AvlInsertR(Dsa_AvlNode *root, Dsa_AvlNode *node, i32 (*cmp)(Dsa_AvlNode *, Dsa_AvlNode *), bool *ok)
{
    if (!root)
        return node;

    i32 c = cmp(node, root);

    if (c < 0)
    {
        root->Left = Dsa_AvlInsertR(root->Left, node, cmp, ok);
        if (root->Left)
            root->Left->Parent = root;
    }
    else if (c > 0)
    {
        root->Right = Dsa_AvlInsertR(root->Right, node, cmp, ok);
        if (root->Right)
            root->Right->Parent = root;
    }
    else
    {
        *ok = false;
        return root;
    }

    return s_AvlRebalance(root);
}

static Dsa_AvlNode *s_AvlFindMin(Dsa_AvlNode *n)
{
    while (n->Left)
        n = n->Left;

    return n;
}

static Dsa_AvlNode *s_AvlRemoveMinHelper(Dsa_AvlNode *root, Dsa_AvlNode **min_node)
{
    if (!root->Left)
    {
        *min_node = root;
        return root->Right;
    }

    root->Left = s_AvlRemoveMinHelper(root->Left, min_node);
    if (root->Left)
        root->Left->Parent = root;

    return s_AvlRebalance(root);
}

Dsa_AvlNode *Dsa_AvlRemoveR(Dsa_AvlNode *root, void *key, i32 (*cmp_key)(void *, Dsa_AvlNode *), Dsa_AvlNode **removed)
{
    if (!root)
        return nullptr;

    i32 c = cmp_key(key, root);

    if (c < 0)
    {
        root->Left = Dsa_AvlRemoveR(root->Left, key, cmp_key, removed);
        if (root->Left)
            root->Left->Parent = root;
    }
    else if (c > 0)
    {
        root->Right = Dsa_AvlRemoveR(root->Right, key, cmp_key, removed);
        if (root->Right)
            root->Right->Parent = root;
    }
    else
    {
        *removed = root;

        if (!root->Left || !root->Right)
        {
            Dsa_AvlNode *child = root->Left ? root->Left : root->Right;
            if (child)
                child->Parent = root->Parent;

            return child;
        }

        Dsa_AvlNode *succ = nullptr;
        root->Right       = s_AvlRemoveMinHelper(root->Right, &succ);

        if (root->Right)
            root->Right->Parent = root;

        succ->Left   = root->Left;
        succ->Right  = root->Right;
        succ->Parent = root->Parent;
        if (succ->Left)
            succ->Left->Parent = succ;

        if (succ->Right)
            succ->Right->Parent = succ;

        *removed = root;
        return s_AvlRebalance(succ);
    }

    return s_AvlRebalance(root);
}

Dsa_AvlNode *Dsa_AvlFindR(Dsa_AvlNode *root, void *key, i32 (*cmp_key)(void *, Dsa_AvlNode *))
{
    while (root)
    {
        i32 c = cmp_key(key, root);
        if (c < 0)
            root = root->Left;
        else if (c > 0)
            root = root->Right;
        else
            return root;
    }

    return nullptr;
}

Dsa_AvlNode *Dsa_AvlNext(Dsa_AvlNode *n)
{
    if (n->Right)
    {
        n = n->Right;
        while (n->Left)
            n = n->Left;

        return n;
    }

    while (n->Parent && n == n->Parent->Right)
        n = n->Parent;

    return n->Parent;
}

Dsa_AvlNode *Dsa_AvlFirst(Dsa_AvlTree *tree)
{
    if (!tree->Root)
        return nullptr;

    return s_AvlFindMin(tree->Root);
}
