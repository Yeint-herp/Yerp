#include <dsa/List.h>

void Dsa_ListInit(Dsa_ListEntry *head)
{
    head->Flink = head;
    head->Blink = head;
}

bool Dsa_ListIsEmpty(const Dsa_ListEntry *head)
{
    return head->Flink == head;
}

void Dsa_ListInsertTail(Dsa_ListEntry *head, Dsa_ListEntry *entry)
{
    entry->Blink       = head->Blink;
    entry->Flink       = head;
    head->Blink->Flink = entry;
    head->Blink        = entry;
}

void Dsa_ListInsertHead(Dsa_ListEntry *head, Dsa_ListEntry *entry)
{
    entry->Flink       = head->Flink;
    entry->Blink       = head;
    head->Flink->Blink = entry;
    head->Flink        = entry;
}

Dsa_ListEntry *Dsa_ListRemoveHead(Dsa_ListEntry *head)
{
    Dsa_ListEntry *entry = head->Flink;
    entry->Flink->Blink  = head;
    head->Flink          = entry->Flink;
    return entry;
}

void Dsa_ListRemoveEntry(Dsa_ListEntry *entry)
{
    entry->Blink->Flink = entry->Flink;
    entry->Flink->Blink = entry->Blink;
    entry->Flink = entry->Blink = nullptr;
}
