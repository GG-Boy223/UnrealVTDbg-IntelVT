#include "IoctlHandlerCommon.h"
#include "../../../Common/Ring0/UserDataPayload.h"
#include "../../Encrypt/Blowfish/Blowfish.h"

USER_DATA GetUserData(PUSER_DATA userData)
{
    USER_DATA user = { 0 };
    if (userData)
    {
        user.Count = userData->Count;
        user.uSize = userData->uSize;
        user.pUserData = userData->pUserData;
    }
    return user;
}

UCHAR* AllocateAndDecryptUserRecords(const USER_DATA& user, size_t recordSize, size_t* recordCount)
{
    if (recordCount != nullptr)
    {
        *recordCount = 0;
    }

    if (recordSize == 0 || user.pUserData == 0 || user.uSize < recordSize ||
        user.uSize > USER_DATA_MAX_PLAINTEXT_SIZE ||
        (user.uSize % recordSize) != 0)
    {
        return nullptr;
    }

    const ULONG alignedSize = AlignUserDataToBlowfishBlock(user.uSize);
    if (alignedSize < user.uSize)
    {
        return nullptr;
    }

    UCHAR* plainText = allocate_pool<UCHAR*>(alignedSize);
    if (plainText == nullptr)
    {
        return nullptr;
    }

    RtlZeroMemory(plainText, alignedSize);
    __try
    {
        DecryptData((PVOID)user.pUserData, plainText);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        free_pool(plainText);
        return nullptr;
    }

    if (recordCount != nullptr)
    {
        *recordCount = user.uSize / recordSize;
    }

    return plainText;
}
