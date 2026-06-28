#include <ntifs.h>
#include "../StringHandler.h"

namespace
{
    constexpr SIZE_T kDefaultFileNameChars = 256;

    bool CopyUnicodeSubstring(OUT WCHAR* dest, IN SIZE_T destChars, IN const WCHAR* source, IN SIZE_T sourceChars)
    {
        if (dest == nullptr || destChars == 0 || source == nullptr)
        {
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        dest[0] = L'\0';
        if (sourceChars == 0 || sourceChars >= destChars)
        {
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        RtlCopyMemory(dest, source, sourceChars * sizeof(WCHAR));
        dest[sourceChars] = L'\0';
        return true;
    }
}

//获取文件名称
bool GetModuleFileName(OUT WCHAR* fileName, IN PUNICODE_STRING filePath)
{
    return GetModuleFileNameSized(fileName, kDefaultFileNameChars, filePath);
}

bool GetModuleFileNameSized(OUT WCHAR* fileName, IN SIZE_T fileNameChars, IN PUNICODE_STRING filePath)
{
    if (fileName == nullptr || fileNameChars == 0 || filePath == nullptr ||
        filePath->Buffer == nullptr || filePath->Length == 0 ||
        (filePath->Length % sizeof(WCHAR)) != 0)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
        //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
        return false;
    }

    const SIZE_T fullLength = filePath->Length / sizeof(WCHAR);
    SIZE_T fileNameStart = fullLength;
    while (fileNameStart > 0 && filePath->Buffer[fileNameStart - 1] != L'\\')
    {
        --fileNameStart;
    }

    const SIZE_T fileNameLength = fullLength - fileNameStart;
    if (fileNameLength == 0)
    {
        fileName[0] = L'\0';
        return false;
    }

    return CopyUnicodeSubstring(fileName, fileNameChars, &filePath->Buffer[fileNameStart], fileNameLength);
}

//获取进程名
NTSTATUS GetProcessName(IN PEPROCESS Process, OUT WCHAR* fileName)
{
    NTSTATUS Status; // eax
    PUNICODE_STRING ImageFileName;
    Status = SeLocateProcessImageName(Process, &ImageFileName);
    if (NT_SUCCESS(Status))
    {
        if (GetModuleFileName(fileName, ImageFileName))
        {
            // /* Then default to STATUS_SUCCESS */
            Status = STATUS_SUCCESS;
        }
        else
        {
            Status = STATUS_UNSUCCESSFUL; // 无异常但未读到数据，返回业务失败。
        }

        if (ImageFileName)
        {
            ExFreePool(ImageFileName);
        }
    }
    // /* Return status */
    return Status;
}
