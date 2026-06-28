#include "DebugEventHandlersInternal.h"

EXTERN_C
VOID DbgkMapViewOfSection(
    IN _EPROCESS* Process,
    IN PVOID SectionObject,
    IN PVOID BaseAddress,
    IN ULONG SectionOffset,
    IN ULONG_PTR ViewSize)
{
    UNREFERENCED_PARAMETER(SectionOffset);
    UNREFERENCED_PARAMETER(ViewSize);

    _TEB* Teb; // rdi
    PIMAGE_NT_HEADERS NtHeaders; // rax
    _ETHREAD* Thread; // r9
    DBGKM_APIMSG ApiMsg = { 0 }; // [rsp+20h] [rbp-118h]
    PDBGKM_LOAD_DLL LoadDllArgs;
    PVOID DebugPort;
    PDEBUG_PROCESS DebugProcess;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    Thread = (_ETHREAD*)PsGetCurrentThread();

    size_t kthread_base = (size_t)Thread + ethread_offset::Tcb;
    size_t ptr_PreviousMode = kthread_base + kthread_offset::PreviousMode;
    if (*(char*)ptr_PreviousMode)
    {
        // /* Use the debug port, unless the thread is being hidden */
        //outLog(("转发异常........\n"));
        if (IsDebugTargetProcess(Process, &DebugProcess))
        {
            DebugPort = DebugProcess->DebugObject;
        }
        else
        {
            size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
            DebugPort = *(PDEBUG_OBJECT*)ptr_DebugPort;
        }

        if (DebugPort)
        {
            //DbgBreakPoint();
            LoadDllArgs = &ApiMsg.u.LoadDll;
            size_t ptr_Teb = kthread_base + kthread_offset::Teb;
            Teb = *(PTEB*)ptr_Teb;

            size_t ptr_Process = kthread_base + kthread_offset::Process;
            if (Teb && (Process == *(_EPROCESS**)ptr_Process))
            {
                if (DbgkpSuppressDbgMsg(Teb))
                {
                    //是我们自己的调试器要创建线程，则直接返回不走线程回调
                    return;
                }

                LoadDllArgs->NamePointer = &Teb->NtTib.ArbitraryUserPointer;
            }
            else
            {
                LoadDllArgs->NamePointer = NULL;
            }

            LoadDllArgs->FileHandle = DbgkpSectionToFileHandle(SectionObject);
            LoadDllArgs->BaseOfDll = BaseAddress;

            __try
            {
                NtHeaders = RtlImageNtHeader(BaseAddress);
                if (NtHeaders)
                {
                    size_t ptr_FileHeader = (size_t)NtHeaders + image_nt_headers64_offset::FileHeader;
                    size_t ptr_PointerToSymbolTable = ptr_FileHeader + image_file_header_offset::PointerToSymbolTable;
                    size_t ptr_NumberOfSymbols = ptr_FileHeader + image_file_header_offset::NumberOfSymbols;
                    LoadDllArgs->DebugInfoFileOffset = *(unsigned long*)ptr_PointerToSymbolTable;
                    LoadDllArgs->DebugInfoSize = *(unsigned long*)ptr_NumberOfSymbols;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                LoadDllArgs->DebugInfoFileOffset = 0;
                LoadDllArgs->DebugInfoSize = 0;
                LoadDllArgs->NamePointer = NULL;
            }

            ApiMsg.h.u1.Length = 0x500028;
            ApiMsg.h.u2.ZeroInit = LPC_DEBUG_EVENT;
            ApiMsg.ApiNumber = DbgKmLoadDllApi;

            DbgkpSendApiMessage(Process, TRUE, &ApiMsg);
            if (LoadDllArgs->FileHandle)
            {
                ObCloseHandle(LoadDllArgs->FileHandle, KernelMode);
            }
        }
    }
}

EXTERN_C
VOID DbgkUnMapViewOfSection(IN _EPROCESS* Process, IN PVOID BaseAddress)
{
    _ETHREAD* Thread; // r8
    PTEB Teb; // rcx
    DBGKM_APIMSG ApiMsg = { 0 }; // [rsp+20h] [rbp-118h]
    PDEBUG_PROCESS DebugProcess;
    PVOID DebugPort;

    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    Thread = (_ETHREAD*)KeGetCurrentThread();
    size_t kthread_base = (size_t)Thread + ethread_offset::Tcb;
    size_t ptr_PreviousMode = kthread_base + kthread_offset::PreviousMode;
    if (*(char*)ptr_PreviousMode)
    {
        // /* Use the debug port, unless the thread is being hidden */
        //outLog(("转发异常........\n"));
        if (IsDebugTargetProcess(Process, &DebugProcess))
        {
            DebugPort = DebugProcess->DebugObject;
        }
        else
        {
            size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
            DebugPort = *(PDEBUG_OBJECT*)ptr_DebugPort;
        }

        if (DebugPort)
        {
            size_t ptr_Teb = kthread_base + kthread_offset::Teb;
            Teb = *(PTEB*)ptr_Teb;

            size_t ptr_Process = kthread_base + kthread_offset::Process;
            if (!Teb || (Process != *(_EPROCESS**)ptr_Process) || (!DbgkpSuppressDbgMsg(Teb)))
            {
                ApiMsg.u.UnloadDll.BaseAddress = BaseAddress;
                ApiMsg.h.u1.Length = 0x380010;
                ApiMsg.h.u2.ZeroInit = LPC_DEBUG_EVENT;
                ApiMsg.ApiNumber = DbgKmUnloadDllApi;
                DbgkpSendApiMessage(Process, TRUE, &ApiMsg);
            }
        }
    }
}
