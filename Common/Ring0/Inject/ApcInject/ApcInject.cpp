#include <ntifs.h>
#include <ntstrsafe.h>
#include "Types/ApcInjectNativeSupportTypes.h"

// OS-dependent data.
DYNAMIC_DATA dynData;

// Implementations are split by responsibility under Core:
// - ApcQueue.cpp / ThreadLookup.cpp: APC injection, callbacks, and thread lookup.
// - ShellcodeBuilder.cpp: x64 shellcode generation.
// - UserModuleLookup.cpp / ApiSetResolver.cpp / ImagePathResolver.cpp / SxsResolver.cpp: module and path resolution.
// - ExportResolver.cpp / RemoteRoutineCall.cpp: export lookup and remote calls.
// - StringUtilities.cpp / KernelResolver.cpp / ThreadExecution.cpp: shared runtime utilities.
