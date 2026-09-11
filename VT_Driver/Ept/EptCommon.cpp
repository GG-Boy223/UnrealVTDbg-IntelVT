#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../msr.h"
#include "../AllocateMem.h"
#include "../vmx.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"
#include "EptCommon.h"

namespace ept
{
	// 常量已在EptCommon.h中定义，此处删除重复定义

	unsigned int normalize_vcpu_count(int requested_count)
	{
		if (requested_count <= 0 || g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
		{
			return 0;
		}

		unsigned int count = static_cast<unsigned int>(requested_count);
		if (count != g_vmm_context.processor_count)
		{
			return 0;
		}

		return count;
	}

	bool is_valid_watch_range(unsigned __int64 virtual_address, unsigned __int64 size)
	{
		if (size == 0 || size > kMaxWatchBytes)
		{
			return false;
		}

		return virtual_address + size >= virtual_address;
	}

	void initialize_hooked_page_runtime_fields(__ept_hooked_page_info* hooked_page_info)
	{
		if (hooked_page_info == nullptr)
		{
			return;
		}

		hooked_page_info->ID = -1;
		hooked_page_info->isBp = false;
		hooked_page_info->isInt3 = false;
		hooked_page_info->last_watch_id = -1;
		hooked_page_info->fake_page_synced = false;
		hooked_page_info->fake_page_needs_full_sync = false;
		hooked_page_info->mtf_reference_count = 0;
		hooked_page_info->removing = 0;
	}

	bool is_watch_id_valid(int ID)
	{
		return ID >= 0 && ID < EPTWATCHLISTSIZE;
	}
}
