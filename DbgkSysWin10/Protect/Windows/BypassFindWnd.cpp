#include "../../Driver.h"
#include "../../ntos/inc/mmtypes.h"
#include "../../ntos/inc/ntdbg.h"
#include "../../ntos/inc/ketypes.h"
#include "../../ntos/inc/extypes.h"
#include "../../ntos/inc/ntosdef.h"
#include "../../ntos/inc/amd64.h"
#include "../../ntos/inc/mi.h"
#include "../../ntos/inc/pstypes.h"
#include "../../ntos/inc/obtypes.h"
#include "../../ntos/inc/peb_teb.h"
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"
#include "BypassFindWnd.h"

//是被保护的窗口
BOOLEAN IsProtectWnd(PUNICODE_STRING WindowName)
{
    PLIST_ENTRY ListHead, NextEntry;
    PWINDOW_DATA fileData;

    if (WindowName && StrIsValid2(*WindowName))
    {
        ExAcquireFastMutex(&g_ProtectWndObjList.Mutex);
        __try
        {
            ListHead = &g_ProtectWndObjList.list_entry.ListHead;
            NextEntry = ListHead->Flink;
            while (ListHead != NextEntry)
            {
                fileData = CONTAINING_RECORD(NextEntry,
                    WINDOW_DATA,
                    list_entry);
                if (RtlUnicodeStringContains(WindowName, &fileData->WindowName, TRUE))
                {
                    return TRUE;
                }
                /* Move to the next entry */
                NextEntry = NextEntry->Flink;
            }
            return FALSE;
        }
        __finally
        {
            ExReleaseFastMutex(&g_ProtectWndObjList.Mutex);
        }
    }
    else
    {
        return FALSE;
    }
}