#include "../../EptHook.h"

namespace ept
{
	//写伪造页内存
	bool write_fake_page_memory(__ept_hooked_function_info* hooked_function_info, void* target_address, void* buffer, unsigned __int64 buffer_size)
	{
		if ((hooked_function_info != NULL) && (target_address != NULL) && (buffer != NULL))
		{
			// Get offset of hooked function within page
			// 获得线性地址的低12位 页偏移
			// 得到欲修改的指令在页内的偏移
			unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_address);
			RtlCopyMemory(&hooked_function_info->fake_page_contents[page_offset], buffer, buffer_size);
		}
		return true;
	}

	//读伪造页内存
	bool read_fake_page_memory(__ept_hooked_function_info* hooked_function_info, void* target_address, void* buffer, unsigned __int64 buffer_size)
	{
		if ((hooked_function_info != NULL) && (target_address != NULL) && (buffer != NULL))
		{
			// Get offset of hooked function within page
			// 获得线性地址的低12位 页偏移
			// 得到欲读取的指令在页内的偏移
			unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_address);
			RtlCopyMemory(buffer, &hooked_function_info->fake_page_contents[page_offset], buffer_size);
		}
		return true;
	}
}
