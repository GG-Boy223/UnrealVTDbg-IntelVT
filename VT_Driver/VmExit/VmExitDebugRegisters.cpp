#include "VmExitControl.h"

namespace
{
	// DR6 固定位：bit 4-11 与 bit 17-31 恒为 1、bit 12 恒为 0（Intel SDM Vol3B 17.2.3）。
	constexpr unsigned __int64 kDr6ActiveLow = 0xFFFF0FF0ull;
	// DR6.BD（bit 13）：debug register access detected，general-detect 条件置位。
	constexpr unsigned __int64 kDr6BdBit = 1ull << 13;
}

// MOV DR 退出处理（mov_dr_exiting = 1，见 Vmcs/VmcsControls.cpp）。
//
// 为什么必须虚拟化 MOV DR：
//   VM-exit 没有 host DR7 字段（Intel SDM Vol3C 27.5.1），root 运行期的物理 DR7 就是 guest 的
//   DR7。guest 一旦把 GD 置 1，ASM/vm-exit.asm 序言/收尾里无条件的 mov drN 会在 root 触发 #DB
//   （SDM Vol3B 17.2.1），落到 root IDT 的三重故障路径 → 宿主蓝屏；而"先在汇编里清 GD"不可行，
//   因为 GD=1 时写 DR7 的 MOV DR 本身就先触发 #DB。唯一正解是让 guest 的 MOV DR 全部进入 root，
//   由本 handler 维护影子，并保证物理 DR7 / GUEST_DR7 永不携带 GD —— 与 KVM handle_dr /
//   kvm_set_dr 同构。
//
// 影子存放位置：
//   DR0-DR3 / DR6 —— 复用 guest_context。vm-exit.asm 序言把物理寄存器存进该结构、收尾再从该结构
//                    写回物理寄存器，因此 handler 必须改 guest_context 而不是直接 __writedr()，
//                    否则写入会被收尾用旧值覆盖。
//   DR7          —— __vcpu::guest_dr7_shadow（物理 DR7 由 VM-entry 从 GUEST_DR7 载入）。
void vmexit_mov_dr_handler(__vcpu* vcpu)
{
	__exit_qualification_dr qualification = { vcpu->vmexit_info.qualification };
	unsigned const debugRegister = (unsigned)qualification.debug_register_number;
	unsigned const generalRegister = (unsigned)qualification.gp_register;
	guest_context* const guest = vcpu->vmexit_info.guest_registers;

	// DR4 / DR5 在 CR4.DE = 0 时不存在，按 SDM 注入 #UD（Windows 不启用已废弃的 CR4.DE）。
	if (debugRegister == 4 || debugRegister == 5)
	{
		hv::inject_interruption(
			EXCEPTION_VECTOR_UNDEFINED_OPCODE,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION,
			0,
			false);
		return;
	}

	// general-detect：guest 曾用 GD=1 写过 DR7。
	// SDM Vol3B 17.2.1：GD=1 时任何 MOV DR 都先产生 #DB，且处理器在进入 #DB 处理程序时清除 GD
	// （一次性语义，与 KVM handle_dr 的 `if (dr7 & DR7_GD)` 分支一致）。GD #DB 属 fault 型，
	// 被检测的指令不执行，因此这里不推进 RIP —— guest 处理完 #DB 后会重新执行本条 MOV DR。
	if ((vcpu->guest_dr7_shadow & kDr7GdBit) != 0)
	{
		vcpu->guest_dr7_shadow &= ~kDr7GdBit;
		hv::vmwrite(GUEST_DR7, sanitize_guest_dr7(vcpu->guest_dr7_shadow));

		// 在既有 DR6 上叠加 BD（其它 B0-B3/BS/BT 位保持 sticky 语义），再补固定位。
		const unsigned __int64 dr6State = guest->dr6 | kDr6BdBit | kDr6ActiveLow;
		hv::inject_interruption_with_debug_state(
			EXCEPTION_VECTOR_SINGLE_STEP,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION,
			0,
			false,
			dr6State);
		return;
	}

	if (qualification.access_direction == 0) // 写 DR
	{
		const unsigned __int64 value = hv::read_guest_gpr(guest, generalRegister);
		switch (debugRegister)
		{
		case 0:
			guest->dr0 = value;
			break;
		case 1:
			guest->dr1 = value;
			break;
		case 2:
			guest->dr2 = value;
			break;
		case 3:
			guest->dr3 = value;
			break;
		case 6:
		{
			// DR6 只有 B0-B3/BD/BS/BT/RTM 可写，其余为固定位；归一化后再存影子。
			__dr6 dr6;
			dr6.all = value;
			dr6.reserved_1 = 0xFF;    // bit 4-11 恒为 1
			dr6.reserved_2 = 0;       // bit 12 恒为 0
			dr6.reserved_3 = 0x7FFF;  // bit 17-31 恒为 1
			dr6.all &= 0xFFFFFFFFull; // bit 63-32 保留为 0
			guest->dr6 = dr6.all;
			break;
		}
		case 7:
		{
			// 影子保留 GD（保证 MOV DR7 读回语义），写入 GUEST_DR7 的值剔除 GD。
			vcpu->guest_dr7_shadow = normalize_guest_dr7_shadow(value);
			hv::vmwrite(GUEST_DR7, sanitize_guest_dr7(value));
			break;
		}
		default:
			hv::inject_interruption(
				EXCEPTION_VECTOR_UNDEFINED_OPCODE,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION,
				0,
				false);
			return;
		}
	}
	else // 读 DR
	{
		unsigned __int64 value = 0;
		switch (debugRegister)
		{
		case 0:
			value = guest->dr0;
			break;
		case 1:
			value = guest->dr1;
			break;
		case 2:
			value = guest->dr2;
			break;
		case 3:
			value = guest->dr3;
			break;
		case 6:
			value = guest->dr6;
			break;
		case 7:
			// 返回含 GD 的影子，而不是 GUEST_DR7（后者被刻意剔除了 GD）。
			value = vcpu->guest_dr7_shadow;
			break;
		default:
			hv::inject_interruption(
				EXCEPTION_VECTOR_UNDEFINED_OPCODE,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION,
				0,
				false);
			return;
		}

		hv::write_guest_gpr(guest, generalRegister, value);
	}

	adjust_rip(vcpu);
}
