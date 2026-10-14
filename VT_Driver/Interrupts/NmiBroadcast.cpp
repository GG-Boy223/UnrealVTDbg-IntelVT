#include "Driver.h"
#include "Globals.h"
#include "invalid_ept.h"
#include "NmiBroadcast.h"

namespace hv
{
	namespace
	{
		constexpr unsigned __int64 kIa32ApicBaseMsr = 0x1B;
		constexpr unsigned __int64 kIa32X2ApicIcrMsr = 0x830;
		constexpr unsigned __int64 kApicIcrOffset = 0x300;
		constexpr unsigned __int64 kApicIcrHighOffset = 0x310;
	}

	// 写 xAPIC/x2APIC ICR 触发 NMI：destination shorthand = all excluding self，
	// delivery mode = NMI，level = 1。
	static void write_icr_nmi(unsigned __int32 low, unsigned __int32 high)
	{
		const unsigned __int64 apic_base = __readmsr(kIa32ApicBaseMsr);

		if (apic_base & (1ull << 10)) // x2APIC 模式
		{
			__writemsr(kIa32X2ApicIcrMsr, (static_cast<unsigned __int64>(high) << 32) | low);
			return;
		}

		// xAPIC 模式：ICR 位于 local APIC base + 0x300/0x310。触发点位于 VMCALL 处理
		// 上下文（IRQL <= DISPATCH_LEVEL），可以安全使用 MmMapIoSpace。
		PHYSICAL_ADDRESS pa;
		pa.QuadPart = apic_base & 0xFFFFFFFFF000ull;
		volatile unsigned __int32* apic = static_cast<volatile unsigned __int32*>(
			MmMapIoSpace(pa, PAGE_SIZE, MmNonCached));
		if (apic == nullptr)
		{
			return;
		}

		*(apic + (kApicIcrHighOffset >> 2)) = high;
		*(apic + (kApicIcrOffset >> 2)) = low;

		MmUnmapIoSpace(const_cast<unsigned __int32*>(apic), PAGE_SIZE);
	}

	void trigger_generic_nmi()
	{
		// delivery mode = NMI (100b) << 8，level = 1 << 14，
		// destination shorthand = all excluding self (11b) << 18。
		const unsigned __int32 icr = (4u << 8) | (1u << 14) | (3u << 18);
		write_icr_nmi(icr, 0);
	}

	static __vcpu* current_vcpu_from_fs()
	{
		return reinterpret_cast<__vcpu*>(_readfsbase_u64());
	}

	static void broadcast_nmi_action(LONG action, unsigned __int64 ept_pointer, bool all_contexts)
	{
		__vcpu* const current = current_vcpu_from_fs();
		for (unsigned int i = 0; i < g_vmm_context.processor_count; i++)
		{
			__vcpu* const vcpu = &g_vmm_context.vcpu[i];
			if (vcpu == current)
			{
				continue;
			}
			InterlockedExchange(&vcpu->nmi_broadcast_action, action);
		}

		// 当前核自己直接 INVEPT，不依赖自己的 NMI。
		if (all_contexts)
		{
			invept_current_context();
		}
		else
		{
			invept_single_context_func(ept_pointer);
		}

		trigger_generic_nmi();
	}

	void broadcast_nmi_invept_all_contexts()
	{
		broadcast_nmi_action(NMI_BROADCAST_ACTION_INVEPT_ALL_CONTEXTS, 0, true);
	}

	void broadcast_nmi_invept_single_context(unsigned __int64 ept_pointer)
	{
		broadcast_nmi_action(NMI_BROADCAST_ACTION_INVEPT_SINGLE_CONTEXT, ept_pointer, false);
	}

	bool handle_nmi_broadcast_action(__vcpu* cpu)
	{
		if (cpu == nullptr)
		{
			return false;
		}

		const LONG action = InterlockedExchange(
			&cpu->nmi_broadcast_action,
			NMI_BROADCAST_ACTION_NONE);

		switch (action)
		{
		case NMI_BROADCAST_ACTION_INVEPT_ALL_CONTEXTS:
			if (cpu->ept_state != nullptr && cpu->ept_state->ept_pointer != nullptr)
			{
				invept_single_context_func(cpu->ept_state->ept_pointer->all);
			}
			return true;

		case NMI_BROADCAST_ACTION_INVEPT_SINGLE_CONTEXT:
			if (cpu->ept_state != nullptr && cpu->ept_state->ept_pointer != nullptr)
			{
				invept_single_context_func(cpu->ept_state->ept_pointer->all);
			}
			return true;

		default:
			return false;
		}
	}
}
