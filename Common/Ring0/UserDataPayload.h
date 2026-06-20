#pragma once

#ifndef _USER_DATA_PAYLOAD_H
#define _USER_DATA_PAYLOAD_H

#include <ntifs.h>
#include <limits.h>
#include <Shared/SharedStruct.h>
#include "Memory/AllocateMem.h"

typedef struct _KERNEL_USER_DATA_PAYLOAD
{
    USER_DATA User;
    PVOID EncryptedData;
    ULONG EncryptedDataBytes;
} KERNEL_USER_DATA_PAYLOAD, *PKERNEL_USER_DATA_PAYLOAD;

inline ULONG AlignUserDataToBlowfishBlock(ULONG size)
{
    constexpr ULONG kBlockSize = 8;
    const ULONG remainder = size % kBlockSize;
    return remainder == 0 ? size : size + (kBlockSize - remainder);
}

inline BOOLEAN GetExpectedEncryptedUserDataBytes(ULONG plainTextBytes, ULONG* encryptedTextBytes)
{
    if (encryptedTextBytes == nullptr ||
        plainTextBytes == 0 ||
        plainTextBytes > USER_DATA_MAX_PLAINTEXT_SIZE)
    {
        return FALSE;
    }

    const ULONG alignedBytes = AlignUserDataToBlowfishBlock(plainTextBytes);
    if (alignedBytes > ((ULONG_MAX - 1) / 2))
    {
        return FALSE;
    }

    *encryptedTextBytes = alignedBytes * 2 + 1;
    return TRUE;
}

inline BOOLEAN IsHexUserDataPayload(const char* payload, ULONG payloadBytes)
{
    if (payload == nullptr || payloadBytes <= 1 || payload[payloadBytes - 1] != '\0')
    {
        return FALSE;
    }

    for (ULONG i = 0; i < payloadBytes - 1; ++i)
    {
        const char value = payload[i];
        const BOOLEAN isHex =
            (value >= '0' && value <= '9') ||
            (value >= 'A' && value <= 'F') ||
            (value >= 'a' && value <= 'f');
        if (!isHex)
        {
            return FALSE;
        }
    }

    return TRUE;
}

inline VOID FreeKernelUserDataPayload(PKERNEL_USER_DATA_PAYLOAD payload)
{
    if (payload == nullptr)
    {
        return;
    }

    free_pool(payload->EncryptedData);
    RtlZeroMemory(payload, sizeof(*payload));
}

inline NTSTATUS ResolveKernelUserDataPayload(PVOID systemBuffer, ULONG inputBufferBytes, PKERNEL_USER_DATA_PAYLOAD payload)
{
    if (payload == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(payload, sizeof(*payload));

    if (systemBuffer == nullptr || inputBufferBytes < sizeof(USER_DATA))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const USER_DATA* inputUser = reinterpret_cast<const USER_DATA*>(systemBuffer);
    payload->User.Count = inputUser->Count;
    payload->User.uSize = inputUser->uSize;
    payload->User.pUserData = 0;

    if (inputUser->uSize == 0)
    {
        return STATUS_SUCCESS;
    }

    ULONG encryptedDataBytes = 0;
    if (!GetExpectedEncryptedUserDataBytes(inputUser->uSize, &encryptedDataBytes))
    {
        return STATUS_INVALID_PARAMETER;
    }

    const void* encryptedSource = nullptr;
    if (inputUser->Count == USER_DATA_INLINE_PACKET_MAGIC)
    {
        if (inputUser->pUserData != sizeof(USER_DATA))
        {
            return STATUS_INVALID_PARAMETER;
        }

        const ULONGLONG requiredInputBytes =
            static_cast<ULONGLONG>(sizeof(USER_DATA)) + encryptedDataBytes;
        if (requiredInputBytes > inputBufferBytes)
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        encryptedSource = static_cast<const UCHAR*>(systemBuffer) + sizeof(USER_DATA);
    }
    else
    {
        if (inputUser->pUserData == 0)
        {
            return STATUS_INVALID_PARAMETER;
        }

        encryptedSource = reinterpret_cast<const void*>(inputUser->pUserData);
    }

    char* encryptedCopy = allocate_pool<char*>(encryptedDataBytes);
    if (encryptedCopy == nullptr)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS status = STATUS_SUCCESS;
    __try
    {
        if (inputUser->Count != USER_DATA_INLINE_PACKET_MAGIC &&
            ExGetPreviousMode() != KernelMode)
        {
            ProbeForRead(const_cast<void*>(encryptedSource), encryptedDataBytes, sizeof(char));
        }

        RtlCopyMemory(encryptedCopy, encryptedSource, encryptedDataBytes);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        status = GetExceptionCode();
    }

    if (!NT_SUCCESS(status) || !IsHexUserDataPayload(encryptedCopy, encryptedDataBytes))
    {
        free_pool(encryptedCopy);
        RtlZeroMemory(payload, sizeof(*payload));
        return NT_SUCCESS(status) ? STATUS_INVALID_PARAMETER : status;
    }

    payload->EncryptedData = encryptedCopy;
    payload->EncryptedDataBytes = encryptedDataBytes;
    payload->User.Count = USER_DATA_KERNEL_BUFFER_MAGIC;
    payload->User.pUserData = reinterpret_cast<ULONG64>(encryptedCopy);
    return STATUS_SUCCESS;
}

#endif // !_USER_DATA_PAYLOAD_H
