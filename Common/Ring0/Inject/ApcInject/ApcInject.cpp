#include <ntifs.h>
#include <ntstrsafe.h>
#include "Types/ApcInjectNativeSupportTypes.h"

DYNAMIC_DATA dynData;

// Implementations are split by responsibility under Core/:
// - ApcQueue.cpp / ThreadLookup.cpp: APC injection, APC callbacks, thread lookup
// - ShellcodeBuilder.cpp: x64 shellcode generation
// - UserModuleLookup.cpp / ApiSetResolver.cpp / ImagePathResolver.cpp / SxsResolver.cpp:
//   module path resolution, API Set, SxS
// - ExportResolver.cpp / RemoteRoutineCall.cpp: export lookup and remote routine calls
// - StringUtilities.cpp / KernelResolver.cpp / ThreadExecution.cpp:
//   string helpers, kernel base, SSDT, pattern search, worker execution
