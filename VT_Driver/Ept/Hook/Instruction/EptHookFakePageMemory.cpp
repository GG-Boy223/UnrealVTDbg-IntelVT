#include "../../EptHook.h"
#include <Ept/EptCommon.h>

namespace ept
{
	// 写伪造页内存:只接受完整落在单个 4KB 伪页内的范围。
	bool write_fake_page_memory(
		__ept_hooked_function_info* hooked_function_info,
		void* target_address,
		void* buffer,
		unsigned __int64 buffer_size)
	{
		if (hooked_function_info == nullptr ||
			hooked_function_info->fake_page_contents == nullptr ||
			target_address == nullptr ||
			buffer == nullptr ||
			buffer_size == 0 ||
			buffer_size > kEptPageSize)
		{
			return false;
		}

		const unsigned __int64 page_offset =
			MASK_EPT_PML1_OFFSET((unsigned __int64)target_address);
		if (page_offset >= kEptPageSize ||
			buffer_size > kEptPageSize - page_offset)
		{
			return false;
		}

		RtlCopyMemory(
			&hooked_function_info->fake_page_contents[page_offset],
			buffer,
			static_cast<SIZE_T>(buffer_size));
		return true;
	}

	// 读伪造页内存:只接受完整落在单个 4KB 伪页内的范围。
	bool read_fake_page_memory(
		__ept_hooked_function_info* hooked_function_info,
		void* target_address,
		void* buffer,
		unsigned __int64 buffer_size)
	{
		if (hooked_function_info == nullptr ||
			hooked_function_info->fake_page_contents == nullptr ||
			target_address == nullptr ||
			buffer == nullptr ||
			buffer_size == 0 ||
			buffer_size > kEptPageSize)
		{
			return false;
		}

		const unsigned __int64 page_offset =
			MASK_EPT_PML1_OFFSET((unsigned __int64)target_address);
		if (page_offset >= kEptPageSize ||
			buffer_size > kEptPageSize - page_offset)
		{
			return false;
		}

		RtlCopyMemory(
			buffer,
			&hooked_function_info->fake_page_contents[page_offset],
			static_cast<SIZE_T>(buffer_size));
		return true;
	}
}
