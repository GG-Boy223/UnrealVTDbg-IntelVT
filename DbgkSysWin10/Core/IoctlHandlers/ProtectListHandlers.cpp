#include "../../Driver.h"
#include "IoctlHandlerCommon.h"
#include "../../Globals.h"
#include "../../List/MyList.h"
#include "../../../Common/Ring0/String/StringHandler.h"

VOID InsertProtectFileList(PFILEDATA fileData)
{
    if (fileData)
    {
        ExAcquireFastMutex(&g_ProtectFileObjList.Mutex);
        InsertNode(&g_ProtectFileObjList.list_entry, &fileData->list_entry);
        ExReleaseFastMutex(&g_ProtectFileObjList.Mutex);
    }
}

VOID InsertProtectWndList(PWINDOW_DATA fileData)
{
    if (fileData)
    {
        ExAcquireFastMutex(&g_ProtectWndObjList.Mutex);
        InsertNode(&g_ProtectWndObjList.list_entry, &fileData->list_entry);
        ExReleaseFastMutex(&g_ProtectWndObjList.Mutex);
    }
}

VOID InitFileList(PRING3_PROTECT_OBJECT pProtectObj)
{
    if (pProtectObj == nullptr || pProtectObj->fileData == 0)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    WCHAR* sText = (WCHAR*)pProtectObj->fileData;

    __try
    {
        for (;;)
        {
            if (sText == NULL)
            {
                break;
            }

            WCHAR* subStr = (WCHAR*)MemAllocate(256 * sizeof(WCHAR), FALSE, TAG_PRO);
            WCHAR* subStr2 = (WCHAR*)MemAllocate(256 * sizeof(WCHAR), FALSE, TAG_PRO);
            PFILEDATA fileData = (PFILEDATA)MemAllocate(sizeof(FILEDATA), FALSE, TAG_PRO);
            BOOLEAN inserted = FALSE;

            if (subStr == NULL || subStr2 == NULL || fileData == NULL)
            {
                if (subStr != NULL)
                {
                    FreeMemAllocate(subStr, TAG_PRO);
                }
                if (subStr2 != NULL)
                {
                    FreeMemAllocate(subStr2, TAG_PRO);
                }
                if (fileData != NULL)
                {
                    FreeMemAllocate(fileData, TAG_PRO);
                }
                outFile("初始化文件列表失败：内存不足.");
                break;
            }

            sText = SplitString(sText, subStr, '&');
            if (StrIsValid(subStr))
            {
                RtlInitUnicodeString(&fileData->fileName, subStr);
            }

            if (sText != NULL)
            {
                sText = SplitString(sText, subStr2, '%');
                if (StrIsValid(subStr2))
                {
                    RtlInitUnicodeString(&fileData->filePath, subStr2);
                }
            }

            if (StrIsValid2(fileData->fileName) && StrIsValid2(fileData->filePath))
            {
                InsertProtectFileList(fileData);
                inserted = TRUE;
            }

            if (!inserted)
            {
                if (subStr != NULL)
                {
                    FreeMemAllocate(subStr, TAG_PRO);
                }
                if (subStr2 != NULL)
                {
                    FreeMemAllocate(subStr2, TAG_PRO);
                }
                FreeMemAllocate(fileData, TAG_PRO);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outFile("初始化文件列表异常.");
    }
}

VOID InitWindowList(PRING3_PROTECT_OBJECT pProtectObj)
{
    if (pProtectObj == nullptr || pProtectObj->fileData == 0)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    WCHAR* sText = (WCHAR*)pProtectObj->fileData;

    __try
    {
        for (;;)
        {
            if (sText == NULL)
            {
                break;
            }

            WCHAR* subStr = (WCHAR*)MemAllocate(256 * sizeof(WCHAR), FALSE, TAG_PRO);
            PWINDOW_DATA fileData = (PWINDOW_DATA)MemAllocate(sizeof(WINDOW_DATA), FALSE, TAG_PRO);
            BOOLEAN inserted = FALSE;

            if (subStr == NULL || fileData == NULL)
            {
                if (subStr != NULL)
                {
                    FreeMemAllocate(subStr, TAG_PRO);
                }
                if (fileData != NULL)
                {
                    FreeMemAllocate(fileData, TAG_PRO);
                }
                outFile("初始化窗口列表失败：内存不足.");
                break;
            }

            sText = SplitString(sText, subStr, '&');
            if (StrIsValid(subStr))
            {
                RtlInitUnicodeString(&fileData->WindowName, subStr);
            }

            if (StrIsValid2(fileData->WindowName))
            {
                InsertProtectWndList(fileData);
                inserted = TRUE;
            }

            if (!inserted)
            {
                FreeMemAllocate(subStr, TAG_PRO);
                FreeMemAllocate(fileData, TAG_PRO);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outFile("初始化窗口列表异常.");
    }
}

//初始化保护列表
VOID InitProtectList(IN PUSER_DATA userData, IN PIRP pIrp)
{
    if (pIrp == nullptr || pIrp->AssociatedIrp.SystemBuffer == nullptr)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    USER_DATA user = GetUserData(userData);
    PRING3_PROTECT_OBJECT output = (PRING3_PROTECT_OBJECT)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(RING3_PROTECT_OBJECT));

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(user, sizeof(RING3_PROTECT_OBJECT), &numElements);
    if (aucPlainText == nullptr)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    ULONG fileCount = 0;
    ULONG windowCount = 0;

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PRING3_PROTECT_OBJECT pInfo = reinterpret_cast<PRING3_PROTECT_OBJECT>(aucPlainText + i * sizeof(RING3_PROTECT_OBJECT));
        ULONG dataType = pInfo->dataType;
        if (dataType == PROTECT_FILE)
        {
            ++fileCount;
            InitFileList(pInfo);
        }
        else if (dataType == PROTECT_WINDOW)
        {
            ++windowCount;
            InitWindowList(pInfo);
        }
    }

    outFile("保护名单已装载: file=%u window=%u", fileCount, windowCount);
    free_pool(aucPlainText);
}
