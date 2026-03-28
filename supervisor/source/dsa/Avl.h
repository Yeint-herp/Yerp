#ifndef SUPERVISOR_DSA_AVL_H
#define SUPERVISOR_DSA_AVL_H

typedef struct Dsa_AvlNode
{
    struct Dsa_AvlNode *Left;
    struct Dsa_AvlNode *Right;
    struct Dsa_AvlNode *Parent;

    i32 Balance;
} Dsa_AvlNode;

typedef struct Dsa_AvlTree
{
    Dsa_AvlNode *Root;

    usize Count;
} Dsa_AvlTree;

#define Dsa_AvlEntry(ptr, Type, Member) container_of(ptr, Type, Member)

#define Dsa_AvlTreeInit(tree)                                                                                          \
    ({                                                                                                                 \
        (tree)->Root  = nullptr;                                                                                       \
        (tree)->Count = 0;                                                                                             \
    })

#define Dsa_AvlInsert(tree, node, cmp)                                                                                 \
    ({                                                                                                                 \
        bool __ok    = true;                                                                                           \
        (node)->Left = (node)->Right = (node)->Parent = nullptr;                                                       \
        (node)->Balance                               = 0;                                                             \
        (tree)->Root                                  = Dsa_AvlInsertR((tree)->Root, (node), (cmp), &__ok);            \
        if (__ok)                                                                                                      \
            (tree)->Count++;                                                                                           \
        __ok;                                                                                                          \
    })

Dsa_AvlNode *Dsa_AvlInsertR(Dsa_AvlNode *root, Dsa_AvlNode *node, i32 (*cmp)(Dsa_AvlNode *, Dsa_AvlNode *), bool *ok);

#define Dsa_AvlRemove(tree, key, cmp_key)                                                                              \
    ({                                                                                                                 \
        Dsa_AvlNode *__removed = nullptr;                                                                              \
        (tree)->Root           = Dsa_AvlRemoveR((tree)->Root, (void *)(key), (cmp_key), &__removed);                   \
        if (__removed)                                                                                                 \
            (tree)->Count--;                                                                                           \
        __removed;                                                                                                     \
    })

#define Dsa_AvlFind(tree, key, cmp_key) Dsa_AvlFindR((tree)->Root, (void *)(key), (cmp_key))

Dsa_AvlNode *Dsa_AvlRemoveR(Dsa_AvlNode *root, void *key, i32 (*cmp_key)(void *, Dsa_AvlNode *), Dsa_AvlNode **removed);
Dsa_AvlNode *Dsa_AvlFindR(Dsa_AvlNode *root, void *key, i32 (*cmp_key)(void *, Dsa_AvlNode *));

Dsa_AvlNode *Dsa_AvlFirst(Dsa_AvlTree *tree);
Dsa_AvlNode *Dsa_AvlNext(Dsa_AvlNode *n);

#define Dsa_AvlForEach(tree, iter)                                                                                     \
    for (Dsa_AvlNode *iter = Dsa_AvlFirst(tree); iter != nullptr; iter = Dsa_AvlNext(iter))

#endif /* SUPERVISOR_DSA_AVL_H */
