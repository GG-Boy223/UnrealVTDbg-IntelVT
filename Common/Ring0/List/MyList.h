#pragma once

#ifndef _MY_LIST_H
#define _MY_LIST_H

typedef struct _MY_LIST
{
    LIST_ENTRY ListHead;
    ULONG Count;
} MY_LIST;

VOID InitializeList(MY_LIST* list, PFAST_MUTEX FastMutex);
VOID InsertNode(MY_LIST* list, LIST_ENTRY* entry);
VOID RemoveNode(MY_LIST* list, LIST_ENTRY* entry);
ULONG GetListCount(MY_LIST* list);


#endif // !_MY_LIST_H
