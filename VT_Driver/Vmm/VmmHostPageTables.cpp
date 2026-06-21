#include "VmmInternal.h"

//创建host页表
bool create_host_page_tables() // 构建宿主页表并映射全部物理内存，保证VMX根模式可访问系统内存
{
	PEPROCESS Process = NULL;
	bool found_dwm = false;
	bool prepared = false;

	if (PsGetNextProcess == nullptr)
	{
		outFile("[失败][Host页表] PsGetNextProcess为空，无法枚举dwm.exe");
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
			// RtlInitUnicodeString(&targetImage, L"dwm.exe"); // 旧注释：目标进程dwm.exe（系统常驻，便于获取稳定地址空间）
			RtlInitUnicodeString(&targetImage, L"dwm.exe"); // 目标进程：dwm.exe（用户要求保留该地址空间来源）
			if (RtlEqualUnicodeString(&ImageFileName, &targetImage, TRUE)) // 找到dwm进程
			{
				found_dwm = true;
				hv::ghv.system_cr3.flags =
					*reinterpret_cast<unsigned __int64*>(
						reinterpret_cast<uint8_t*>(Process) + hv::ghv.kprocess_directory_table_base_offset);

				outFile("[信息][Host页表] 命中dwm.exe：EPROCESS=0x%p DirectoryTableBase=0x%llX",
					Process,
					hv::ghv.system_cr3.flags);

				if (hv::ghv.system_cr3.flags == 0)
				{
					outFile("[失败][Host页表] dwm.exe DirectoryTableBase为0，停止构建Host页表");
					break;
				}

				KAPC_STATE ApcState;
				bool attached = false;

				__try
				{
					KeStackAttachProcess(Process, &ApcState); // 进入目标进程地址空间，保持原dwm.exe页表构建方式
					attached = true;
					__try
					{
						prepared = hv::prepare_host_page_tables(); // 构建宿主页表（可能触发异常）
					}
					__except (EXCEPTION_EXECUTE_HANDLER)
					{
						outFile("[失败][Host页表] prepare_host_page_tables触发异常：Code=0x%X", GetExceptionCode());
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
		outFile("[失败][Host页表] 未找到dwm.exe，无法准备Host页表");
	}

	if (!prepared)
	{
		RtlSecureZeroMemory(&hv::ghv.host_page_tables, sizeof(hv::ghv.host_page_tables));
		hv::ghv.system_cr3.flags = 0;
		outFile("[失败][Host页表] Host页表构建失败，已清空Host页表状态");
		return false;
	}

	//hv::ghv.system_cr3.flags = hv::get_system_directory_table_base(); // 旧备用路径：切到System CR3；当前按用户要求必须保留dwm.exe来源
	// hv::prepare_host_page_tables();
	// return prepared;
	outFile("[成功][Host页表] Host页表构建完成：来源=dwm.exe PhysicalMapGB=%u PML4_PA=0x%llX",
		HOST_PHYSICAL_MEMORY_PD_COUNT,
		MmGetPhysicalAddress(&hv::ghv.host_page_tables.pml4).QuadPart);
	return true;
}
