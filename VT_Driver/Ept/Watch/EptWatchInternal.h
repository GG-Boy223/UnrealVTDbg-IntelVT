#pragma once

#include "../EptWatch.h"
#include "../EptBuild.h"
#include "../EptSync.h"
#include "../EptCommon.h"
#include "../../poolmanager.h"
#include "../../mtrr.h"
#include "../../EPT.h"
#include "../../msr.h"
#include "../../AllocateMem.h"
#include "../../vmx.h"
#include "../../hypervisor_routines.h"
#include "../../Vmcs/VmcsApi.h"
#include "../../Cr3/Cr3Tracker.h"

namespace ept
{
	void prepare_int3_fake_page_entries(__ept_hooked_page_info* hooked_page_info);
	bool sync_int3_fake_page_from_guest(__ept_hooked_page_info* hooked_page_info, cr3 guest_cr3, unsigned __int64 virtual_address);
}
