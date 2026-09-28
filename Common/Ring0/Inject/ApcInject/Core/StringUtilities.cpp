#include <ntifs.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "UtilityFunctions.h"

#pragma warning(disable: 4996) // Preserve legacy pool API for down-level kernel compatibility.

/// </summary>
/// <param name="path">Path.</param>
/// <param name="name">Resulting name</param>
/// <returns>Status code</returns>
/// <summary>
/// Get file name from full path
NTSTATUS BBStripPath(IN PUNICODE_STRING path, OUT PUNICODE_STRING name)
{
    ASSERT(path != NULL && name);
    if (path == NULL || name == NULL || path->Buffer == NULL || (path->Length % sizeof(WCHAR)) != 0)
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。

    // Empty string
    if (path->Length < 2)
    {
        *name = *path;
        // *name = *path;
        return STATUS_NOT_FOUND;
    }

    for (USHORT i = (path->Length / sizeof(WCHAR)) - 1; i != 0; i--)
    {
        if (path->Buffer[i] == L'\\' || path->Buffer[i] == L'/')
        {
            name->Buffer = &path->Buffer[i + 1];
            name->Length = name->MaximumLength = path->Length - (i + 1) * sizeof(WCHAR);
            return STATUS_SUCCESS;
        }
    }

    *name = *path;
    // *name = *path;
    return STATUS_NOT_FOUND;
}

/// <param name="source">Source string</param>
/// <param name="target">Target string</param>
/// <param name="CaseInSensitive">Case insensitive search</param>
/// <returns>Found position or -1 if not found</returns>
/// <summary>
/// Search for substring
/// </summary>
LONG BBSafeSearchString(IN PUNICODE_STRING source, IN PUNICODE_STRING target, IN BOOLEAN CaseInSensitive)
{
    ASSERT(source != NULL && target != NULL);
    if (source == NULL || target == NULL || source->Buffer == NULL || target->Buffer == NULL)
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。

    // Size mismatch
    if (source->Length < target->Length)
        return -1;

    USHORT diff = source->Length - target->Length;
    for (USHORT i = 0; i <= (diff / sizeof(WCHAR)); i++)
    {
        if (RtlCompareUnicodeStrings(
            source->Buffer + i,
            target->Length / sizeof(WCHAR),
            target->Buffer,
            target->Length / sizeof(WCHAR),
            CaseInSensitive
        ) == 0)
        {
            return i;
        }
    }

    return -1;
}

/// </summary>
/// <param name="result">Resulting string</param>
/// <param name="source">Source string</param>
/// <returns>Status code</returns>
/// <summary>
/// Allocate and copy string
NTSTATUS BBSafeInitString(OUT PUNICODE_STRING result, IN PUNICODE_STRING source)
{
    ASSERT(result != NULL && source != NULL);
    if (result == NULL || source == NULL || source->Buffer == NULL ||
        source->Length > source->MaximumLength ||
        (source->Length % sizeof(WCHAR)) != 0)
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。

    result->Length = 0;
    result->MaximumLength = 0;
    result->Buffer = NULL;

    // No data to copy
    if (source->Length == 0)
    {
        return STATUS_SUCCESS;
    }

    const SIZE_T allocation_size = static_cast<SIZE_T>(source->Length) + sizeof(WCHAR);
    if (allocation_size > MAXUSHORT)
        return STATUS_INVALID_PARAMETER;

    result->Buffer = (PWCH)ExAllocatePoolWithTag(PagedPool, allocation_size, BB_POOL_TAG);
    if (result->Buffer == NULL)
        return STATUS_NO_MEMORY;

    result->Length = source->Length;
    result->MaximumLength = static_cast<USHORT>(allocation_size);

    memcpy(result->Buffer, source->Buffer, source->Length);
    result->Buffer[source->Length / sizeof(WCHAR)] = L'\0';

    return STATUS_SUCCESS;
}

/// </summary>
/// <param name="result">Resulting string</param>
/// <param name="size">Buffer size in bytes to alloacate</param>
/// <returns>Status code</returns>
/// <summary>
/// Allocate new Unicode string from Paged pool
NTSTATUS BBSafeAllocateString(OUT PUNICODE_STRING result, IN USHORT size)
{
    ASSERT(result != NULL);
    if (result == NULL || size < sizeof(WCHAR) || (size % sizeof(WCHAR)) != 0)
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。

    result->Length = 0;
    result->MaximumLength = 0;
    result->Buffer = NULL;

    result->Buffer = (PWCH)ExAllocatePoolWithTag(PagedPool, size, BB_POOL_TAG);
    result->Length = 0;
    result->MaximumLength = size;

    if (result->Buffer)
        RtlZeroMemory(result->Buffer, size);
    else
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

/// </summary>
/// <param name="path">Path</param>
/// <param name="name">Resulting directory path</param>
/// <returns>Status code</returns>
/// <summary>
/// Get directory path name from full path
NTSTATUS BBStripFilename(IN PUNICODE_STRING path, OUT PUNICODE_STRING dir)
{
    ASSERT(path != NULL && dir);
    if (path == NULL || dir == NULL || path->Buffer == NULL || (path->Length % sizeof(WCHAR)) != 0)
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。

    // Empty string
    if (path->Length < 2)
    {
        *dir = *path;
        // *dir = *path;
        return STATUS_NOT_FOUND;
    }

    for (USHORT i = (path->Length / sizeof(WCHAR)) - 1; i != 0; i--)
    {
        if (path->Buffer[i] == L'\\' || path->Buffer[i] == L'/')
        {
            dir->Buffer = path->Buffer;
            dir->Length = dir->MaximumLength = i * sizeof(WCHAR);
            return STATUS_SUCCESS;
        }
    }

    *dir = *path;
    // *dir = *path;
    return STATUS_NOT_FOUND;
}

/// Check if file exists
/// </summary>
/// <param name="path">Fully qualifid path to a file</param>
/// <returns>Status code</returns>
/// <summary>
NTSTATUS BBFileExists(IN PUNICODE_STRING path)
{
    HANDLE hFile = NULL;
    IO_STATUS_BLOCK statusBlock = { 0 };
    OBJECT_ATTRIBUTES obAttr = { 0 };
    InitializeObjectAttributes(&obAttr, path, OBJ_KERNEL_HANDLE, NULL, NULL);

    NTSTATUS status = ZwCreateFile(
        &hFile, FILE_READ_DATA | SYNCHRONIZE, &obAttr,
        &statusBlock, NULL, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

    if (NT_SUCCESS(status))
        ZwClose(hFile);

    return status;
}
