#include <ntifs.h>
#include <intrin.h>
#include "Common.h"

namespace Common
{
	bool isIntel = false;
	bool isAMD = false;

	//确认CPU型号
	void ConfirmCPUVendor()
	{
		int cpui[4];		// EAX, EBX, ECX, EDX

		// Calling __cpuid with 0x0 as the function_id argument
		// gets the number of the highest valid function ID.
		__cpuid(cpui, 0);

		if (cpui[ebx] == 0x756e6547 && cpui[ecx] == 0x6c65746e && cpui[edx] == 0x49656e69)  //GenuineIntel
		{
			isIntel = true;
		}
		else if (cpui[ebx] == 0x68747541 && cpui[ecx] == 0x444d4163 && cpui[edx] == 0x69746e65)  //AuthenticAMD
		{
			isAMD = true;
		}
	}

	//检测Intel CPU是否支持VT虚拟化技术
	BOOLEAN CheckVTSupported()
	{
		int cpui[4];		// EAX, EBX, ECX, EDX

		__cpuid(cpui, 1);
		if (cpui[ecx] & CPUID_1_ECX_VMX)  // check VMX bit
		{

			return TRUE;
		}

		return FALSE;
	}

	//检测主板VT是否已启用
	BOOLEAN CheckVTEnabled()
	{
		ULONG_PTR msr;

		msr = __readmsr(IA32_FEATURE_CONTROL_CODE);
		if (msr & FEATURE_CONTROL_VMXON_ENABLED_OUTSIDE_SMX)
		{
			return TRUE;
		}
		return FALSE;
	}
}