#pragma once
#include "../Driver.h"
#include "../Globals.h"

// EPT Watch 监视点管理
namespace ept
{
	// ept_watch_activate_internal / hide_cc_breakpoint 的细分失败原因。
	// 回传给来宾时编码为 EPT_WATCH_DETAIL_ERROR_BASE + detail，避免和
	// 顶层 99..105 错误码混淆。
	enum EPT_WATCH_FAILURE_DETAIL : int
	{
		EPT_WATCH_DETAIL_NONE = 0,
		EPT_WATCH_DETAIL_PHYSICAL_ADDRESS = 1,
		EPT_WATCH_DETAIL_PREPARE_PML2 = 2,
		EPT_WATCH_DETAIL_SPLIT_POOL_EMPTY = 3,
		EPT_WATCH_DETAIL_SPLIT_FAILED = 4,
		EPT_WATCH_DETAIL_PML1_MISSING = 5,
		EPT_WATCH_DETAIL_PAGE_POOL_EMPTY = 6,
		EPT_WATCH_DETAIL_FAKE_PAGE_SYNC = 7,
		EPT_WATCH_DETAIL_COMMIT = 8,
		EPT_WATCH_DETAIL_OPTION_CONFLICT = 9
	};

	constexpr int EPT_WATCH_DETAIL_ERROR_BASE = 120;

	inline int ept_watch_detail_error(int detail)
	{
		return detail > EPT_WATCH_DETAIL_NONE
			? EPT_WATCH_DETAIL_ERROR_BASE + detail
			: 0;
	}

	// 判断 watch 记录的 CR3 root 是否匹配当前 guest CR3。低 12 位是 PCID，
	// 不能参与 root 比较；watch_cr3==0 保留为全局 watch 通配。
	inline bool watch_cr3_matches(unsigned __int64 watch_cr3, unsigned __int64 guest_cr3)
	{
		const unsigned __int64 watch_root = watch_cr3 & ~0xfffull;
		const unsigned __int64 guest_root = guest_cr3 & ~0xfffull;
		return watch_root == 0 || (guest_root != 0 && watch_root == guest_root);
	}

	// 读取当前 guest ETHREAD.Cid.UniqueProcess。符号偏移不可用/内存不可读时
	// 返回 0，调用方必须回退到 CR3 匹配。
	inline unsigned __int64 current_guest_process_id()
	{
		PCLIENT_ID cid = GuestCurrentThreadCid();
		if (cid == nullptr)
		{
			return 0;
		}

		CLIENT_ID localCid = {};
		if (hv::read_guest_virtual_memory(
			hv::ghv.system_cr3,
			cid,
			&localCid,
			sizeof(localCid)) != sizeof(localCid))
		{
			return 0;
		}

		return static_cast<unsigned __int64>(
			reinterpret_cast<ULONG_PTR>(localCid.UniqueProcess));
	}

	// PID 可用时优先用 PID 判断进程身份（共享内核高半区/共享页表仍能区分）；
	// 否则回退到 CR3 root 比较。
	inline bool watch_process_matches(
		const EPTWatchEntry& entry,
		unsigned __int64 current_pid,
		unsigned __int64 guest_cr3)
	{
		if (entry.pid != 0 && current_pid != 0)
		{
			return entry.pid == current_pid;
		}

		return watch_cr3_matches(entry.cr3, guest_cr3);
	}

	// 隐藏软件断点
	//读取隐形软件断点
	bool get_hide_software_breakpoint(__ept_state& ept_state, PVT_BREAK_POINT vmcallinfo);
	bool hide_cc_breakpoint(__ept_state& ept_state, VT_BREAK_POINT vmcallinfo, unsigned __int64 physical_address, unsigned __int64 Type, int* failure_detail = nullptr);
	bool commit_hide_cc_breakpoint(__ept_state& ept_state, unsigned __int64 virtual_address, unsigned __int64 physical_address);
	//设置隐形软件断点
	bool set_hide_software_breakpoint(__vcpu* vcpu, PVT_BREAK_POINT vmcallinfo);

	// EPT watch 激活和停用
	void set_ept_watch(__ept_hooked_page_info* hooked_page_info, unsigned __int64 Type);
	void apply_active_watch_permissions_locked(__ept_hooked_page_info* hooked_page_info, int ignoredID);
	bool ept_watch_activate_internal(__ept_state& ept_state, unsigned __int64 physical_address, unsigned __int64 Type, int* failure_detail = nullptr);
	//设置ept监视
	bool ept_watch_activate(__vcpu* vcpu, VT_BREAK_POINT vmcallinfo, unsigned __int64 Type, PVT_BREAK_POINT guest_request, int& errorCode);
	bool ept_watch_deactivate_internal(__vcpu* vcpu, __ept_state& ept_state, unsigned __int64 VirtualAddress, unsigned __int64 physical_address, unsigned __int64 Type,
		int bpType, unsigned char OriginalByte, int has, int ignoredID);
	//取消监视断点
	int ept_watch_deactivate(__vcpu* vcpu, VT_BREAK_POINT vmcallinfo, int ID);

	// 页面事件处理
	void fillPageEventBasic(PageEventBasic* peb, guest_context* guest_registers);
	void RestoreEptPageProperties(__ept_hooked_page_info* hooked_page_info, int ID);

	// 23/24:按 guest 虚拟地址和 CR3 读写隐藏 INT3 对应的单页 fake page 内容。
	bool get_software_breakpoint_buffer(
		__ept_state& ept_state,
		unsigned __int64 target_address,
		void* buffer,
		unsigned __int64 buffer_size,
		unsigned __int64 current_cr3);
	bool set_software_breakpoint_buffer(
		__ept_state& ept_state,
		unsigned __int64 target_address,
		void* buffer,
		unsigned __int64 buffer_size,
		unsigned __int64 current_cr3);

	// Watch 查询
	int ept_isWatchAddress(unsigned __int64 guest_physical_adddress, int ID);
	//判断是否是监视的页
	int ept_isWatchPage(unsigned __int64 pfn, int ID);
	int ept_getWatchID(unsigned __int64 guest_physical_adddress);

	// 断点清理
	void clear_detected_breakpoint_for_watch_locked(unsigned __int64 virtual_address);

	// Watch 事件处理
	bool ept_handleWatchEvent(__vcpu* vcpu, __ept_violation ept_violation, __ept_hooked_page_info* hooked_page_info,
		unsigned __int64 guest_physical_adddress, int& bpType);
	bool ept_handle_int3_execute(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info,
		unsigned __int64 guest_physical_address);
}
