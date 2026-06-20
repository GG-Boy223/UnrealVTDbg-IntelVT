#include <ntifs.h>
#include <ntstrsafe.h>
#include "../../../PE/PE_struct.h"
#include "../Api/ApcInjectNativeApi.h"
#include "ModuleResolver.h"
#include "UtilityFunctions.h"

PVOID BBGetModuleExport(IN PVOID pBase, IN PCCHAR name_ord, IN PEPROCESS pProcess, IN PUNICODE_STRING baseName)
{
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)pBase;
    PIMAGE_NT_HEADERS32 pNtHdr32 = NULL;
    PIMAGE_NT_HEADERS64 pNtHdr64 = NULL;
    PIMAGE_EXPORT_DIRECTORY pExport = NULL;
    ULONG expSize = 0;
    ULONG_PTR pAddress = 0;

    ASSERT(pBase != NULL);
    if (pBase == NULL)
        return NULL;

    if (pDosHdr->e_magic != IMAGE_DOS_SIGNATURE)
        return NULL;

    pNtHdr32 = (PIMAGE_NT_HEADERS32)((PUCHAR)pBase + pDosHdr->e_lfanew);
    pNtHdr64 = (PIMAGE_NT_HEADERS64)((PUCHAR)pBase + pDosHdr->e_lfanew);

    if (pNtHdr32->Signature != IMAGE_NT_SIGNATURE)
        return NULL;

    if (pNtHdr32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        pExport = (PIMAGE_EXPORT_DIRECTORY)(pNtHdr64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress + (ULONG_PTR)pBase);
        expSize = pNtHdr64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    }
    else
    {
        pExport = (PIMAGE_EXPORT_DIRECTORY)(pNtHdr32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress + (ULONG_PTR)pBase);
        expSize = pNtHdr32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    }

    PUSHORT pAddressOfOrds = (PUSHORT)(pExport->AddressOfNameOrdinals + (ULONG_PTR)pBase);
    PULONG pAddressOfNames = (PULONG)(pExport->AddressOfNames + (ULONG_PTR)pBase);
    PULONG pAddressOfFuncs = (PULONG)(pExport->AddressOfFunctions + (ULONG_PTR)pBase);

    for (ULONG i = 0; i < pExport->NumberOfFunctions; ++i)
    {
        USHORT OrdIndex = 0xFFFF;
        PCHAR pName = NULL;

        if ((ULONG_PTR)name_ord <= 0xFFFF)
        {
            OrdIndex = (USHORT)i;
        }
        else if ((ULONG_PTR)name_ord > 0xFFFF && i < pExport->NumberOfNames)
        {
            pName = (PCHAR)(pAddressOfNames[i] + (ULONG_PTR)pBase);
            OrdIndex = pAddressOfOrds[i];
        }
        else
            return NULL;

        if (((ULONG_PTR)name_ord <= 0xFFFF && (USHORT)((ULONG_PTR)name_ord) == OrdIndex + pExport->Base) ||
            ((ULONG_PTR)name_ord > 0xFFFF && strcmp(pName, name_ord) == 0))
        {
            pAddress = pAddressOfFuncs[OrdIndex] + (ULONG_PTR)pBase;

            if (pAddress >= (ULONG_PTR)pExport && pAddress <= (ULONG_PTR)pExport + expSize)
            {
                WCHAR strbuf[256] = { 0 };
                ANSI_STRING forwarder = { 0 };
                ANSI_STRING import = { 0 };
                UNICODE_STRING uForwarder = { 0 };
                ULONG delimIdx = 0;
                PVOID forwardBase = NULL;
                PVOID result = NULL;

                if (pProcess == NULL)
                    return NULL;

                RtlInitAnsiString(&forwarder, (PCSZ)pAddress);
                RtlInitEmptyUnicodeString(&uForwarder, strbuf, sizeof(strbuf));

                RtlAnsiStringToUnicodeString(&uForwarder, &forwarder, FALSE);
                for (ULONG j = 0; j < uForwarder.Length / sizeof(WCHAR); j++)
                {
                    if (uForwarder.Buffer[j] == L'.')
                    {
                        uForwarder.Length = (USHORT)(j * sizeof(WCHAR));
                        uForwarder.Buffer[j] = L'\0';
                        delimIdx = j;
                        break;
                    }
                }

                RtlInitAnsiString(&import, forwarder.Buffer + delimIdx + 1);
                RtlAppendUnicodeToString(&uForwarder, L".dll");

                UNICODE_STRING resolved = { 0 };
                UNICODE_STRING resolvedName = { 0 };
                BBResolveImagePath(NULL, pProcess, KApiShemaOnly, &uForwarder, baseName, &resolved);
                BBStripPath(&resolved, &resolvedName);

                forwardBase = BBGetUserModule(pProcess, &resolvedName, PsGetProcessWow64Process(pProcess) != NULL);
                result = BBGetModuleExport(forwardBase, import.Buffer, pProcess, &resolvedName);
                RtlFreeUnicodeString(&resolved);

                return result;
            }

            break;
        }
    }

    return (PVOID)pAddress;
}