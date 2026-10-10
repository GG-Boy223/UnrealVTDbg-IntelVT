#include "../Driver.h"
#include "../ntos/inc/mmtypes.h"
#include "../ntos/inc/ntdbg.h"
#include "../ntos/inc/ketypes.h"
#include "../ntos/inc/extypes.h"
#include "../ntos/inc/ntosdef.h"
#include "../ntos/inc/amd64.h"
#include "../ntos/inc/mi.h"
#include "../ntos/inc/pstypes.h"
#include "../ntos/inc/obtypes.h"
#include "../ntos/inc/peb_teb.h"
#include "../List/MyList.h"
#include "../ntos/inc/ntlpcapi.h"
#include "../ntos/inc/psp.h"
#include "../Globals.h"
#include "../DbgkApi/DbgkApi.h"
#include "Callbacks.h"

#define _Altitude_ L"321000"

//在保护对象列表中查找pid  检测是否是被保护的进程
//BOOLEAN IsProtectProcess(HANDLE ProcessId)
//{
//	IsDebugger();
//	BOOLEAN boIs = FALSE;
//	PLIST_ENTRY ListHead, NextEntry;
//	PROTECT_OBJECT_ENTRY* protect_entry;
//
//	ExAcquireFastMutex(&g_ProtectObjectList.Mutex);
//	ListHead = &g_ProtectObjectList.EventList.ListHead;
//	NextEntry = ListHead->Flink;
//	while (ListHead != NextEntry)
//	{
//		protect_entry = CONTAINING_RECORD(NextEntry,
//			PROTECT_OBJECT_ENTRY,
//			EventList);
//
//		if (protect_entry)
//		{
//			if (ProcessId == (HANDLE)protect_entry->dwProcessId)
//			{
//				//是保护的进程对象
//				boIs = TRUE;
//				break;
//			}
//		}
//
//		/* Move to the next entry */
//		NextEntry = NextEntry->Flink;
//	}
//	ExReleaseFastMutex(&g_ProtectObjectList.Mutex);
//	return boIs;
//}


//线程回调
//OB_PREOP_CALLBACK_STATUS preThreadCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
//{
//	UNREFERENCED_PARAMETER(RegistrationContext);
//	if (IsDebugger(PsGetThreadProcess((PETHREAD)pOperationInformation->Object)))
//	{
//		//PrintProcessName(PsGetCurrentProcess());
//
//		//获取当前调用进程
//		UNICODE_STRING ImageFileName, PassImage;
//		NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &ImageFileName);
//		if (NT_SUCCESS(Status))
//		{
//			for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//			{
//				RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//				if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//				{
//					//发现是白名单进程就退出
//					return OB_PREOP_SUCCESS;
//				}
//			}
//		}
//
//		if (pOperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)  //打开句柄
//		{
//			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & THREAD_SUSPEND_RESUME) == THREAD_SUSPEND_RESUME)
//			{
//				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~THREAD_SUSPEND_RESUME;
//			}
//
//			//if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & THREAD_GET_CONTEXT) == THREAD_GET_CONTEXT)
//			//{
//			//	pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~THREAD_GET_CONTEXT;
//			//}
//		}
//		else if (pOperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE)  //复制句柄
//		{
//			if ((pOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess & THREAD_SUSPEND_RESUME) == THREAD_SUSPEND_RESUME)
//			{
//				pOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~THREAD_SUSPEND_RESUME;
//			}
//		}
//	}
//	return OB_PREOP_SUCCESS;
//}


//注意此保护方案无法拦截FindWindow
OB_PREOP_CALLBACK_STATUS preProcessCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading() || pOperationInformation == NULL)
    {
        return OB_PREOP_SUCCESS;
    }

	UNREFERENCED_PARAMETER(RegistrationContext);
	if (IsDebugger((PEPROCESS)pOperationInformation->Object))
	{
		if (pOperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)  //打开句柄
		{
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_READ) == PROCESS_VM_READ)
			{
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
			}
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_WRITE) == PROCESS_VM_WRITE)
			{
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
			}
		}
		else if (pOperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE)  //复制句柄
		{
			if ((pOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess & PROCESS_VM_READ) == PROCESS_VM_READ)
			{
				pOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
			}
			if ((pOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess & PROCESS_VM_WRITE) == PROCESS_VM_WRITE)
			{
				pOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
			}
		}

		if (pOperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)
		{
			ULONG originalAccess = (ULONG)pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess;
			if ((originalAccess & (PROCESS_VM_READ | PROCESS_VM_WRITE)) != 0)
			{
				//outFile("句柄保护(创建): callerPid=%u 调试器pid=%u access 0x%X -> 0x%X",(ULONG)(ULONG_PTR)PsGetCurrentProcessId(),(ULONG)(ULONG_PTR)PsGetProcessId((PEPROCESS)pOperationInformation->Object),originalAccess,(ULONG)pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess);
			}
		}
		else if (pOperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE)
		{
			ULONG originalAccess = (ULONG)pOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess;
			if ((originalAccess & (PROCESS_VM_READ | PROCESS_VM_WRITE)) != 0)
			{
				//outFile("句柄保护(复制): callerPid=%u 调试器pid=%u access 0x%X -> 0x%X",(ULONG)(ULONG_PTR)PsGetCurrentProcessId(),(ULONG)(ULONG_PTR)PsGetProcessId((PEPROCESS)pOperationInformation->Object),originalAccess,(ULONG)pOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess);
			}
		}
	}
	return OB_PREOP_SUCCESS;
}

//设置进程回调
VOID SetProcessCallbacks(IN PDRIVER_OBJECT pDriver_Object)
{
	NTSTATUS Status; // eax
	OB_OPERATION_REGISTRATION oor;
	OB_CALLBACK_REGISTRATION ocr;

	PLDR_DATA_TABLE_ENTRY ldr;
	ldr = (PLDR_DATA_TABLE_ENTRY)pDriver_Object->DriverSection;
	ldr->Flags |= 0x20;//加载驱动的时候会判断此值。必须有特殊签名才行，增加0x20即可。否则将调用失败 

	oor.ObjectType = PsProcessType;
	oor.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	oor.PreOperation = (POB_PRE_OPERATION_CALLBACK)preProcessCallback;
	oor.PostOperation = NULL;

	ocr.Version = OB_FLT_REGISTRATION_VERSION;
	ocr.OperationRegistrationCount = 1;
	ocr.OperationRegistration = &oor;
	RtlInitUnicodeString(&ocr.Altitude, _Altitude_);
	ocr.RegistrationContext = NULL;

	Status = ObRegisterCallbacks(&ocr, &g_obProcessHandle);
	// /* Check if we failed */
	if (!NT_SUCCESS(Status))
	{
		ASSERT(FALSE);
	}
}

//卸载进程回调
VOID UnProcessCallbacks()
{
	ASSERT(g_obProcessHandle);
	if (g_obProcessHandle)
	{
		ObUnRegisterCallbacks(g_obProcessHandle);
	}
}
