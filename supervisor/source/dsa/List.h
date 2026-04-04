#ifndef SUPERVISOR_DSA_LIST_H
#define SUPERVISOR_DSA_LIST_H

typedef struct Dsa_ListEntry
{
    struct Dsa_ListEntry *Flink;
    struct Dsa_ListEntry *Blink;
} Dsa_ListEntry;

void Dsa_ListInit(Dsa_ListEntry *head);
bool Dsa_ListIsEmpty(const Dsa_ListEntry *head);

void Dsa_ListInsertTail(Dsa_ListEntry *head, Dsa_ListEntry *entry);
void Dsa_ListInsertHead(Dsa_ListEntry *head, Dsa_ListEntry *entry);

Dsa_ListEntry *Dsa_ListRemoveHead(Dsa_ListEntry *head);
void           Dsa_ListRemoveEntry(Dsa_ListEntry *entry);

#define Dsa_ListEntry(ptr, Type, Member) container_of(ptr, Type, Member)

#define Dsa_ListForEach(head, iter) for (Dsa_ListEntry *iter = (head)->Flink; iter != (head); iter = iter->Flink)

#define Dsa_ListForEachSafe(head, iter, tmp)                                                                           \
    for (Dsa_ListEntry *iter = (head)->Flink, *tmp = iter->Flink; iter != (head); iter = tmp, tmp = iter->Flink)

#endif /* SUPERVISOR_DSA_LIST_H */
