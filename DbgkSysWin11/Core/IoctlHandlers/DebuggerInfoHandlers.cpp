#include "../../Driver.h"
#include "IoctlHandlerCommon.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"

VOID InitDebuggerInfo(IN PUSER_DATA userData)
{
    USER_DATA user = GetUserData(userData);
    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(user, sizeof(RING3_DEBUGGER_TABLE_ENTRY), &numElements);
    if (aucPlainText == nullptr)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PRING3_DEBUGGER_TABLE_ENTRY pInfo = reinterpret_cast<PRING3_DEBUGGER_TABLE_ENTRY>(aucPlainText + i * sizeof(RING3_DEBUGGER_TABLE_ENTRY));
        if (pInfo->dwPid == 0)
        {
            // *ppThread = NULL;
            continue;
        }

        PDEBUGGER_TABLE_ENTRY debugger = allocate_pool<DEBUGGER_TABLE_ENTRY>();
        if (debugger != nullptr)
        {
            RtlZeroMemory(debugger, sizeof(DEBUGGER_TABLE_ENTRY));
            debugger->dwPid = pInfo->dwPid;
            debugger->fileData = pInfo->fileData;
            debugger->fileData2 = pInfo->fileData2;
            if (!InsertDebuggerList(debugger))
            {
                free_pool(debugger);
            }
        }
    }

    free_pool(aucPlainText);
}
