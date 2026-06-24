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

	// for (Process = PsGetNextProcess(NULL);
	// 	Process != NULL;
	// 	Process = PsGetNextProcess(Process)) // 旧逻辑：枚举到的PEPROCESS引用未释放，长期加载/失败重试会泄漏引用。
	Process = PsGetNextProcess(NULL);
	while (Process != NULL)
	{
		PEPROCESS current_process = Process;
		Process = nullptr;
		WCHAR SubStr[256] = { 0 }; // 进程名缓冲区，用于匹配目标进程
		UNICODE_STRING ImageFileName, targetImage; // 进程名与目标名
		NTSTATUS Status = GetProcessName(current_process, &SubStr[0]);     // 获取进程映像名
		if (NT_SUCCESS(Status))
		{
			RtlInitUnicodeString(&ImageFileName, SubStr); // 初始化当前进程名字符串
			// RtlInitUnicodeString(&targetImage, L"dwm.exe"); // 旧注释：目标进程dwm.exe（系统常驻，便于获取稳定地址空间）
			RtlInitUnicodeString(&targetImage, L"dwm.exe"); // 目标进程：dwm.exe（用户要求保留该地址空间来源）
			if (RtlEqualUnicodeString(&ImageFileName, &targetImage, TRUE)) // 找到dwm进程
			{
				found_dwm = true;
				unsigned __int64 eprocess_directory_table_base =
					*reinterpret_cast<unsigned __int64*>(
						reinterpret_cast<uint8_t*>(current_process) + hv::ghv.kprocess_directory_table_base_offset); // 旧来源：EPROCESS.DirectoryTableBase，仅保留为诊断对照。

				outFile("[信息][Host页表] 命中dwm.exe：EPROCESS=0x%p DirectoryTableBase=0x%llX",
					current_process,
					eprocess_directory_table_base);

				KAPC_STATE ApcState;
				bool attached = false;

				__try
				{
					KeStackAttachProcess(current_process, &ApcState); // 进入目标进程地址空间，保持原dwm.exe页表构建方式
					attached = true;
					__try
					{
						// hv::ghv.system_cr3.flags = eprocess_directory_table_base; // 旧逻辑：只使用EPROCESS字段，KVA/PCID/新系统上可能不是当前硬件实际CR3。
						hv::ghv.system_cr3.flags = __readcr3(); // 新逻辑：attach到dwm.exe后读取当前硬件CR3，保证Host页表来源与实际dwm地址空间一致。
						outFile("[信息][Host页表] dwm.exe CR3=0x%llX EPROCESS_DTB=0x%llX CR4=0x%llX",
							hv::ghv.system_cr3.flags,
							eprocess_directory_table_base,
							__readcr4());

						if (hv::ghv.system_cr3.flags == 0 || hv::ghv.system_cr3.address_of_page_directory == 0)
						{
							outFile("[失败][Host页表] dwm.exe Attach后真实CR3无效，停止构建Host页表");
							prepared = false;
						}
						else
						{
							prepared = hv::prepare_host_page_tables(); // 构建宿主页表（可能触发异常）
						}
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

				ObDereferenceObject(current_process); // PsGetNextProcess返回带引用对象，命中dwm后也必须释放。
				break;
			}
		}

		Process = PsGetNextProcess(current_process); // 先取下一个进程，再释放当前枚举引用，避免链表遍历对象提前失效。
		ObDereferenceObject(current_process);
	}

	if (!found_dwm)
	{
		outFile("[失败][Host页表] 未找到dwm.exe，无法准备Host页表");
	}

	if (!prepared)
	{
		// RtlSecureZeroMemory(&hv::ghv.host_page_tables, sizeof(hv::ghv.host_page_tables)); // 旧逻辑：直接清零会丢失动态页表页指针，导致失败重试泄漏。
		hv::cleanup_host_page_tables();
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
