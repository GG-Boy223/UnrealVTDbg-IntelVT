#pragma once

#include "../../../Driver.h"
#include "../../../ntos/inc/extypes.h"
#include "../../../ntos/inc/ketypes.h"
#include "../../../ntos/inc/ntosdef.h"
#include "../../../ntos/inc/amd64.h"
#include "../../../ntos/inc/mi.h"
#include "../../../ntos/inc/pstypes.h"
#include "../../../ntos/inc/obtypes.h"
#include "../../../ntos/inc/mmtypes.h"
#include "../../../ntos/inc/ntdbg.h"
#include "../../../ntos/inc/peb_teb.h"
#include "../../../List/MyList.h"
#include "../../../ntos/inc/ntlpcapi.h"
#include "../../../Log/log.h"
#include "../../../ntos/inc/ki.h"
#include "../../../ntos/inc/psp.h"
#include "../../../Globals.h"
#include "../../../DbgkApi/DbgkApi.h"
#include "../../../Protect/Windows/BypassFindWnd.h"
#include "../../../Protect/Thread/ProtectDrxX64.h"
#include "../../../Protect/Process/ProtectProcess.h"
#include "../../../Memory/ReadWrite.h"
#include "../../../ntos/inc/ntexapi.h"
#include "../../../Hvm/hypervisor_gateway.h"
#include "../../../Init/Symbolic/InitWin32kbase.h"
#include "../../../Init/Symbolic/InitWin32kfull.h"
#include "../../../Process/Process.h"
#include "../EptHook.h"

template <typename TSystemRoutine, typename THookRoutine>
inline VOID HookEptRoutine(TSystemRoutine SystemRoutine,
    THookRoutine HookRoutine,
    PVOID* OriginalRoutine,
    const char* HookName,
    const char* SymbolName)
{
    ASSERT(SystemRoutine);
    if (SystemRoutine)
    {
        if (hvgt::hook_function(SystemRoutine, HookRoutine, OriginalRoutine))
        {
            outLog("hook %s成功.", HookName);
        }
        else
        {
            outLog("hook %s失败.", HookName);
        }
    }
    else
    {
        outLog("Sys_%s为空指针.", SymbolName);
    }
}
