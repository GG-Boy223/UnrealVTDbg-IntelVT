#include "VmmInternal.h"

//创建host页表
bool create_host_page_tables() // 构建宿主页表并映射全部物理内存，保证VMX根模式可访问系统内存
{
	PEPROCESS Process = NULL;
	bool found_dwm = false;
	bool prepared = false;

	if (PsGetNextProcess == nullptr)
	{
		outDebug("PsGetNextProcess 为空。\n");
		return false;
	}

	for (Process = PsGetNextProcess(NULL);
		Process != NULL;
		Process = PsGetNextProcess(Process))
	{
		WCHAR SubStr[256] = { 0 }; // 进程名缓冲区，用于匹配目标进程
		UNICODE_STRING ImageFileName, targetImage; // 进程名与目标名
		NTSTATUS Status = GetProcessName(Process, &SubStr[0]);     // 获取进程映像名
		if (NT_SUCCESS(Status))
		{
			RtlInitUnicodeString(&ImageFileName, SubStr); // 初始化当前进程名字符串
			RtlInitUnicodeString(&targetImage, L"dwm.exe"); // 目标进程：dwm.exe（系统常驻，便于获取稳定地址空间）
			if (RtlEqualUnicodeString(&ImageFileName, &targetImage, TRUE)) // 找到dwm进程
			{
				found_dwm = true;
				hv::ghv.system_cr3.flags =
					*reinterpret_cast<unsigned __int64*>(
						reinterpret_cast<uint8_t*>(Process) + hv::ghv.kprocess_directory_table_base_offset);

				if (hv::ghv.system_cr3.flags == 0)
				{
					outDebug("dwm.exe DirectoryTableBase 为 0。\n");
					break;
				}

				KAPC_STATE ApcState;
				bool attached = false;

				__try
				{
					KeStackAttachProcess(Process, &ApcState); // 进入目标进程地址空间
					attached = true;
					__try
					{
						prepared = hv::prepare_host_page_tables(); // 构建宿主页表（可能触发异常）
					}
					__except (EXCEPTION_EXECUTE_HANDLER)
					{
						outDebug("prepare_host_page_tables 触发异常。\n");
						prepared = false;
					}
				}
				__finally
				{
					if (attached)
					{
						KeUnstackDetachProcess(&ApcState); // 无条件恢复原进程上下文，避免ApcStateIndex=1
					}
				}

				break;
			}
		}
	}

	if (!found_dwm)
	{
		outDebug("未找到 dwm.exe，无法准备宿主页表。\n");
	}

	if (!prepared)
	{
		RtlSecureZeroMemory(&hv::ghv.host_page_tables, sizeof(hv::ghv.host_page_tables));
		hv::ghv.system_cr3.flags = 0;
	}

	//hv::ghv.system_cr3.flags = hv::get_system_directory_table_base();
	//hv::prepare_host_page_tables();
	return prepared;
}
