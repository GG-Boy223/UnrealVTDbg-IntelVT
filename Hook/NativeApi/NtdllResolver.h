#pragma once

#ifndef _NTDLL_RESOLVER_H
#define _NTDLL_RESOLVER_H

#include "dllmain.h"
#include "Globals.h"

typedef VOID(NTAPI* PFN_DBGUSERBREAKPOINT)();

inline FARPROC ResolveNtdllProcedure(const char* procedureName)
{
    if (procedureName == nullptr || procedureName[0] == '\0')
    {
        return nullptr;
    }

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr)
    {
        return nullptr;
    }

    return GetProcAddress(ntdll, procedureName);
}

inline PFN_NTDEBUGACTIVEPROCESS ResolveNtDebugActiveProcess()
{
    static PFN_NTDEBUGACTIVEPROCESS procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_NTDEBUGACTIVEPROCESS>(
            ResolveNtdllProcedure("NtDebugActiveProcess"));
    }
    return procedure;
}

inline PFN_NTREMOVEPROCESSDEBUG ResolveNtRemoveProcessDebug()
{
    static PFN_NTREMOVEPROCESSDEBUG procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_NTREMOVEPROCESSDEBUG>(
            ResolveNtdllProcedure("NtRemoveProcessDebug"));
    }
    return procedure;
}

inline PFN_NTTERMINATEPROCESS ResolveNtTerminateProcess()
{
    static PFN_NTTERMINATEPROCESS procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_NTTERMINATEPROCESS>(
            ResolveNtdllProcedure("NtTerminateProcess"));
    }
    return procedure;
}

inline PFN_DBGUIISSUEREMOTEBREAKIN ResolveDbgUiIssueRemoteBreakin()
{
    static PFN_DBGUIISSUEREMOTEBREAKIN procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_DBGUIISSUEREMOTEBREAKIN>(
            ResolveNtdllProcedure("DbgUiIssueRemoteBreakin"));
    }
    return procedure;
}

inline PFN_DBGUIDEBUGACTIVEPROCESS ResolveDbgUiDebugActiveProcess()
{
    static PFN_DBGUIDEBUGACTIVEPROCESS procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_DBGUIDEBUGACTIVEPROCESS>(
            ResolveNtdllProcedure("DbgUiDebugActiveProcess"));
    }
    return procedure;
}

inline PFN_NTCREATEUSERPROCESS ResolveNtCreateUserProcess()
{
    static PFN_NTCREATEUSERPROCESS procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_NTCREATEUSERPROCESS>(
            ResolveNtdllProcedure("NtCreateUserProcess"));
    }
    return procedure;
}

inline PFN_NTDEBUGCONTINUE ResolveNtDebugContinue()
{
    static PFN_NTDEBUGCONTINUE procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_NTDEBUGCONTINUE>(
            ResolveNtdllProcedure("NtDebugContinue"));
    }
    return procedure;
}

inline PFN_NTWRITEVIRTUALMEMORY ResolveNtWriteVirtualMemory()
{
    static PFN_NTWRITEVIRTUALMEMORY procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_NTWRITEVIRTUALMEMORY>(
            ResolveNtdllProcedure("NtWriteVirtualMemory"));
    }
    return procedure;
}

inline PFN_NTREADVIRTUALMEMORY ResolveNtReadVirtualMemory()
{
    static PFN_NTREADVIRTUALMEMORY procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_NTREADVIRTUALMEMORY>(
            ResolveNtdllProcedure("NtReadVirtualMemory"));
    }
    return procedure;
}

inline PFN_DBGUSERBREAKPOINT ResolveDbgUserBreakPoint()
{
    static PFN_DBGUSERBREAKPOINT procedure = nullptr;
    if (procedure == nullptr)
    {
        procedure = reinterpret_cast<PFN_DBGUSERBREAKPOINT>(
            ResolveNtdllProcedure("DbgUserBreakPoint"));
    }
    return procedure;
}

inline ULONG64 ResolveDbgUserBreakPointAddress()
{
    return reinterpret_cast<ULONG64>(ResolveDbgUserBreakPoint());
}

#endif // !_NTDLL_RESOLVER_H
