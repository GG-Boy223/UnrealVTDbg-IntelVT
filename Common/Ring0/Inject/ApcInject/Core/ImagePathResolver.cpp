#include <ntifs.h>
#include <ntstrsafe.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "../Api/ApcInjectNativeApi.h"
#include "ModuleResolver.h"
#include "UtilityFunctions.h"

/// <param name="path">Image name to resolve</param>
/// <param name="baseImage">Base image name for API SET translation</param>
/// <param name="resolved">Resolved image path</param>
/// <returns>Status code</returns>
/// <summary>
/// Resolve image name to fully qualified path
/// </summary>
/// <param name="pContext">Loader context</param>
/// <param name="pProcess">Target process. Must be running in the context of this process</param>
/// <param name="flags">Flags</param>
NTSTATUS BBResolveImagePath(
    IN PMMAP_CONTEXT pContext,
    IN PEPROCESS pProcess,
    IN ResolveFlags flags,
    IN PUNICODE_STRING path,
    IN PUNICODE_STRING baseImage,
    OUT PUNICODE_STRING resolved
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING pathLow = { 0 };
    UNICODE_STRING filename = { 0 };
    UNICODE_STRING fullResolved = { 0 };

    UNREFERENCED_PARAMETER(baseImage);

    ASSERT(pProcess != NULL && path != NULL && resolved != NULL);
    if (pProcess == NULL || path == NULL || resolved == NULL)
    {
        outLog("BBResolveImagePath 参数无效");
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
    }

    RtlDowncaseUnicodeString(&pathLow, path, TRUE);
    BBStripPath(&pathLow, &filename);

    // API Schema
    if (NT_SUCCESS(BBResolveApiSet(pProcess, &filename, baseImage, resolved)))
    {
        BBSafeAllocateString(&fullResolved, 512);

        // Prepend x64 system directory.
        RtlUnicodeStringCatString(&fullResolved, L"\\SystemRoot\\system32\\");

        RtlUnicodeStringCat(&fullResolved, resolved);
        RtlFreeUnicodeString(resolved);
        RtlFreeUnicodeString(&pathLow);

        *resolved = fullResolved;
        // *resolved = fullResolved;
        return STATUS_SUCCESS;
    }

    // Api schema only
    if (flags & KApiShemaOnly)
        goto skip;

    if (flags & KSkipSxS)
        goto SkipSxS;

    // SxS
    status = BBResolveSxS(pContext, &filename, resolved);
    if (pContext && NT_SUCCESS(status))
    {
        BBSafeAllocateString(&fullResolved, 1024);
        RtlUnicodeStringCatString(&fullResolved, L"\\??\\");
        RtlUnicodeStringCat(&fullResolved, resolved);

        RtlFreeUnicodeString(resolved);
        RtlFreeUnicodeString(&pathLow);

        *resolved = fullResolved;
        // *resolved = fullResolved;
        return STATUS_SUCCESS;
    }
    else if (status == STATUS_UNHANDLED_EXCEPTION)
    {
        *resolved = pathLow;
        // *resolved = pathLow;
        return status;
    }
    else
        // *handle_table = *entry;
        status = STATUS_SUCCESS;

SkipSxS:
    BBSafeAllocateString(&fullResolved, 0x400);

    //
    // Executable directory
    //
    ULONG bytes = 0;
    if (NT_SUCCESS(ZwQueryInformationProcess(ZwCurrentProcess(), ProcessImageFileName, fullResolved.Buffer + 0x100, 0x200, &bytes)))
    {
        PUNICODE_STRING pPath = (PUNICODE_STRING)(fullResolved.Buffer + 0x100);
        UNICODE_STRING parentDir = { 0 };
        BBStripFilename(pPath, &parentDir);

        RtlCopyUnicodeString(&fullResolved, &parentDir);
        RtlUnicodeStringCatString(&fullResolved, L"\\");
        RtlUnicodeStringCat(&fullResolved, &filename);

        if (NT_SUCCESS(BBFileExists(&fullResolved)))
        {
            RtlFreeUnicodeString(resolved);
            RtlFreeUnicodeString(&pathLow);

            *resolved = fullResolved;
            // *resolved = fullResolved;
            return STATUS_SUCCESS;
        }
    }

    fullResolved.Length = 0;
    RtlZeroMemory(fullResolved.Buffer, 0x400);

    //
    // System directory
    //
    RtlUnicodeStringCatString(&fullResolved, L"\\SystemRoot\\System32\\");

    RtlUnicodeStringCat(&fullResolved, &filename);
    if (NT_SUCCESS(BBFileExists(&fullResolved)))
    {
        RtlFreeUnicodeString(resolved);
        RtlFreeUnicodeString(&pathLow);

        *resolved = fullResolved;
        // *resolved = fullResolved;
        return STATUS_SUCCESS;
    }

    RtlFreeUnicodeString(&fullResolved);

    // Nothing found
skip:
    *resolved = pathLow;
    // *resolved = pathLow;
    return status;
}
