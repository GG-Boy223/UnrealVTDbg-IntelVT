#include "VmmInternal.h"

//创建host页表
bool create_host_page_tables() // 构建宿主页表并映射全部物理内存,保证VMX根模式可访问系统内存
{
	PEPROCESS Process = NULL;
	bool found_dwm = false;
	bool prepared = false;

	if (PsGetNextProcess == nullptr)
	{
		outFile("[失败][Host页表] PsGetNextProcess为空,无法枚举dwm.exe");
		return false;
	}

	// PsGetNextProcess返回的是进程链表的借用指针,不增加引用;只有后续显式引用成功后才配对ObDereferenceObject。
	Process = PsGetNextProcess(NULL);
	while (Process != NULL)
	{
		PEPROCESS current_process = Process;
		Process = PsGetNextProcess(current_process); // 先取下一个进程,再继续使用当前指针
		WCHAR SubStr[256] = { 0 }; // 进程名缓冲区,用于匹配目标进程
		UNICODE_STRING ImageFileName, targetImage; // 进程名与目标名
		NTSTATUS Status = GetProcessName(current_process, &SubStr[0]);     // 获取进程映像名
		if (NT_SUCCESS(Status))
		{
			RtlInitUnicodeString(&ImageFileName, SubStr); // 初始化当前进程名字符串
			RtlInitUnicodeString(&targetImage, L"dwm.exe"); // 目标进程:dwm.exe(用户要求保留该地址空间来源)
			if (RtlEqualUnicodeString(&ImageFileName, &targetImage, TRUE)) // 找到dwm进程
			{
				found_dwm = true;
				outFile("[信息][Host页表] 命中dwm.exe:EPROCESS=0x%p",
					current_process);

				KAPC_STATE ApcState = {};
				bool attached = false;
				bool process_referenced = false;

				__try // 保护性异常处理,避免内核崩溃
				{
					NTSTATUS reference_status = ObReferenceObjectByPointer(
						current_process,
						0,
						*PsProcessType,
						KernelMode);
					if (!NT_SUCCESS(reference_status))
					{
						outFile("[失败][Host页表] dwm.exe引用失败:Status=0x%X", reference_status);
					}
					else
					{
						process_referenced = true;
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					outFile("[失败][Host页表] dwm.exe引用触发异常:Code=0x%X", GetExceptionCode());
				}

				if (!process_referenced)
				{
					break;
				}

				__try // 保护性异常处理,避免内核崩溃
				{
					__try // 保护性异常处理,避免内核崩溃
					{
						KeStackAttachProcess(current_process, &ApcState); // 进入目标进程地址空间,保持原dwm.exe页表构建方式
						attached = true;
						__try // 保护性异常处理,避免内核崩溃
						{
							// hv::ghv.system_cr3.flags = eprocess_directory_table_base; // 旧逻辑:只使用EPROCESS字段,KVA/PCID/新系统上可能不是当前硬件实际CR3。
							hv::ghv.system_cr3.flags = __readcr3(); // 新逻辑:attach到dwm.exe后读取当前硬件CR3,保证Host页表来源与实际dwm地址空间一致。
							outFile("[信息][Host页表] dwm.exe CR3=0x%llX CR4=0x%llX",
								hv::ghv.system_cr3.flags,__readcr4());

							if (hv::ghv.system_cr3.flags == 0 || hv::ghv.system_cr3.address_of_page_directory == 0)
							{
								outFile("[失败][Host页表] dwm.exe Attach后真实CR3无效,停止构建Host页表");
								prepared = false;
							}
							else
							{
								prepared = hv::prepare_host_page_tables(); // 构建宿主页表(可能触发异常)
							}
						}
						__except (EXCEPTION_EXECUTE_HANDLER)
						{
							outFile("[失败][Host页表] prepare_host_page_tables触发异常:Code=0x%X", GetExceptionCode());
							prepared = false;
						}
					}
					__finally
					{
						if (attached)
						{
							KeUnstackDetachProcess(&ApcState); // 无条件恢复原进程上下文,避免ApcStateIndex=1
						}
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					outFile("[失败][Host页表] dwm.exe Attach阶段异常:Code=0x%X", GetExceptionCode());
					prepared = false;
				}
				if (process_referenced)
				{
					ObDereferenceObject(current_process); // 只释放命中dwm.exe后显式增加的一次引用
				}
				break;
			}
		}
	}

	if (!found_dwm)
	{
		outFile("[失败][Host页表] 未找到dwm.exe,无法准备Host页表");
	}

	if (!prepared)
	{
		// RtlSecureZeroMemory(&hv::ghv.host_page_tables, sizeof(hv::ghv.host_page_tables)); // 旧逻辑:直接清零会丢失动态页表页指针,导致失败重试泄漏。
		hv::cleanup_host_page_tables();
		hv::ghv.system_cr3.flags = 0;
		outFile("[失败][Host页表] Host页表构建失败,已清空Host页表状态");
		return false;
	}

	//hv::ghv.system_cr3.flags = hv::get_system_directory_table_base(); // 旧备用路径:切到System CR3;当前按用户要求必须保留dwm.exe来源
	//hv::prepare_host_page_tables();
	// return prepared;
	outFile("[成功][Host页表] Host页表构建完成:来源=dwm.exe PhysicalMapGB=%u PML4_PA=0x%llX",
		HOST_PHYSICAL_MEMORY_PD_COUNT,
		MmGetPhysicalAddress(&hv::ghv.host_page_tables.pml4).QuadPart);
	return true;
}
