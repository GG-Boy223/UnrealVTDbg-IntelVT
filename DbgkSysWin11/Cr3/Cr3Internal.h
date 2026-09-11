#pragma once

#include "Cr3Resolver.h"
#include "../Driver.h"
#include "../Globals.h"

constexpr SIZE_T kCr3ImageProbeSize = 32;

struct CR3_PROCESS_PROBE
{
    PEPROCESS process;
    HANDLE processId;
    unsigned __int64 imageBase;
    UCHAR imageBytes[kCr3ImageProbeSize];
    BOOLEAN hasImageProbe;
};

unsigned __int64 Cr3NormalizeCandidate(unsigned __int64 value);
BOOLEAN Cr3BuildProcessProbe(PEPROCESS process, CR3_PROCESS_PROBE* probe);
BOOLEAN Cr3ValidateCandidate(
    unsigned __int64 candidateCr3,
    const CR3_PROCESS_PROBE* probe);
unsigned __int64 Cr3ScanPhysicalMemory(
    const CR3_PROCESS_PROBE* probe,
    unsigned __int64* scannedPages);
