#include <ntifs.h>
#include "MyList.h"


VOID InitializeList(MY_LIST* list, PFAST_MUTEX FastMutex)
{
    //³õÊ¼»¯»¥³âËø
    ExInitializeFastMutex(FastMutex);
    InitializeListHead(&list->ListHead);
    list->Count = 0;
}

VOID InsertNode(MY_LIST* list, LIST_ENTRY* entry)
{
    InsertTailList(&list->ListHead, entry);
    list->Count++;
}

VOID RemoveNode(MY_LIST* list, LIST_ENTRY* entry)
{
    RemoveEntryList(entry);
    list->Count--;
}

ULONG GetListCount(MY_LIST* list)
{
    return list->Count;
}