#pragma once

#ifndef _NMI_BROADCAST_H
#define _NMI_BROADCAST_H

// 跨核 NMI 广播动作（对齐 HyperDbg 的 NMI_BROADCAST_ACTION_TYPE 语义）。
// host NMI handler 在 vmx-root 态收到 NMI 后，先检查本核 action，命中即执行，
// 否则按普通硬件 NMI 转发给 guest。
//
// 【已知缺陷，当前不可用于跨核 INVEPT】
// trigger_generic_nmi() 发出的 NMI 到达「guest 态」的核时，会因 nmi_exiting 触发 VM-exit，
// 走 VmExitEventInjection.cpp 的 vmexit_exception_or_nmi_handler；该路径只做 queued_nmis++
// 并把 NMI 转发回 guest，**不检查 nmi_broadcast_action**。于是广播 NMI 被当作真实硬件 NMI
// 投递给 Windows（KiNmiInterrupt -> HalHandleNMI -> WheaReportHwError），触发 0x80
// NMI_HARDWARE_FAILURE 蓝屏（实测 F:\MiniDmp\101026-13656-01.dmp）。
//
// 只有 vmx-root 态的核才会走 handle_host_interrupt -> handle_nmi_broadcast_action 消费 action。
// 要真正复用这套 NMI 广播，必须同时在 vmexit_exception_or_nmi_handler 里先消费
// nmi_broadcast_action（命中就执行 INVEPT 并 return，不转发 guest），否则不得调用。
// 当前跨核 INVEPT 统一走 DbgkSysWin 的 DPC 广播 + 每核 single-context，不使用本机制。
enum NMI_BROADCAST_ACTION : LONG
{
	NMI_BROADCAST_ACTION_NONE = 0,
	NMI_BROADCAST_ACTION_INVEPT_ALL_CONTEXTS = 1,
	NMI_BROADCAST_ACTION_INVEPT_SINGLE_CONTEXT = 2,
};

namespace hv
{
	// 通过 APIC ICR 触发一次 NMI 到除当前核以外的所有逻辑处理器（xAPIC/x2APIC 双模式）。
	void trigger_generic_nmi();

	// 当前核立即 INVEPT，其它核通过 NMI 广播在其 host NMI handler 里 INVEPT。
	void broadcast_nmi_invept_all_contexts();
	void broadcast_nmi_invept_single_context(unsigned __int64 ept_pointer);

	// host NMI handler 调用：消费并执行本核的广播 action，返回 true 表示本 NMI 是广播。
	bool handle_nmi_broadcast_action(__vcpu* cpu);
}

#endif // !_NMI_BROADCAST_H
