#include "Cr3Internal.h"

unsigned __int64 Cr3NormalizeCandidate(unsigned __int64 value)
{
    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 0x80000008);
    const ULONG physicalBits = static_cast<ULONG>(cpuInfo[0] & 0xff);
    if (physicalBits < 36 || physicalBits > 52)
    {
        return 0;
    }

    const unsigned __int64 validMask = (1ull << physicalBits) - 1;
    if ((value & ~validMask) != 0)
    {
        return 0;
    }

    const unsigned __int64 root = value & ~0xfffull;
    return root != 0 ? root : 0;
}

BOOLEAN Cr3BuildProcessProbe(PEPROCESS process, CR3_PROCESS_PROBE* probe)
{
    if (process == NULL || probe == NULL || Sys_MmCopyVirtualMemory == NULL)
    {
        return FALSE;
    }

    RtlZeroMemory(probe, sizeof(*probe));
    probe->process = process;
    probe->processId = PsGetProcessId(process);
    __try
    {
        probe->imageBase = *reinterpret_cast<unsigned __int64*>(
            reinterpret_cast<PUCHAR>(process) + eprocess_offset::SectionBaseAddress);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }

    if (probe->processId == NULL ||
        probe->imageBase == 0 ||
        probe->imageBase >= 0x0000800000000000ull)
    {
        return FALSE;
    }

    SIZE_T transferred = 0;
    const NTSTATUS status = Sys_MmCopyVirtualMemory(
        reinterpret_cast<_EPROCESS*>(process),
        reinterpret_cast<PVOID>(probe->imageBase),
        reinterpret_cast<_EPROCESS*>(PsGetCurrentProcess()),
        probe->imageBytes,
        sizeof(probe->imageBytes),
        KernelMode,
        &transferred);
    const BOOLEAN copied = NT_SUCCESS(status) &&
        transferred == sizeof(probe->imageBytes);
    if (!copied)
    {
        return TRUE;
    }

    probe->hasImageProbe =
        *reinterpret_cast<PUSHORT>(probe->imageBytes) == IMAGE_DOS_SIGNATURE;
    return probe->hasImageProbe;
}
