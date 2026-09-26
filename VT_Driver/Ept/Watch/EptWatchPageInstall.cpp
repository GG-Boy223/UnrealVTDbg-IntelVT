#include "EptWatchInternal.h"

namespace ept
{
    void set_ept_watch(__ept_hooked_page_info* hooked_page_info, unsigned __int64 Type)
    {
        if (Type == EPTW_WRITE)
        {
            hooked_page_info->entry_address->write = 0;
        }
        else if (Type == EPTW_READWRITE)
        {
            hooked_page_info->entry_address->read = 0;
            hooked_page_info->entry_address->write = 0;
        }
        else if (Type == EPTW_EXECUTE)
        {
            hooked_page_info->entry_address->execute = 0;
        }

        hooked_page_info->Options = EPTO_VIRTUAL_BREAKPOINT;
    }

    // 调用方必须持有 eptWatchList_lock。该函数读取全局 watch 表并按当前有效条目
    // 重算页面权限；页面生命周期由调用方的热引用固定。
    void apply_active_watch_permissions_locked(
        __ept_hooked_page_info* hooked_page_info,
        int ignoredID)
    {
        if (hooked_page_info == nullptr || hooked_page_info->entry_address == nullptr)
        {
            return;
        }

        hooked_page_info->entry_address->read = 1;
        hooked_page_info->entry_address->write = 1;
        hooked_page_info->entry_address->execute = 1;

        // 重算权限的同时必须把 entry 的 PFN 归一到原页。
        // 隐藏 INT3 用「原页 + 伪页」双映射实现：伪页（changed_entry）只在
        // ept_handle_int3_execute 的命中窗口内被指向，其余时刻 entry 必须落在原页上。
        // 原实现只改 read/write/execute 三个权限位、不写 page_frame_number，因此只要 entry
        // 曾被切到伪页，本函数就会把权限重算作用在伪页上 —— entry 从此长期停在伪页，
        // 伪页内容与原页分叉，删除断点后 guest 仍会在伪页上命中断点字节，
        // 表现为「断点已删除、被调试进程仍然卡」。
        hooked_page_info->entry_address->page_frame_number = hooked_page_info->pfn_of_hooked_page;

        const unsigned __int64 pfn = hooked_page_info->pfn_of_hooked_page;
        bool has_effective_int3 = false;
        int ignored_group = -1;
        if (ignoredID >= 0 && ignoredID < EPTWATCHLISTSIZE &&
            ept_watch_entry_is_reserved(eptWatchList[ignoredID]))
        {
            ignored_group = eptWatchList[ignoredID].GroupId;
        }

        for (int index = 0; index < EPTWATCHLISTSIZE; index++)
        {
            if (index == ignoredID ||
                (ignored_group >= 0 && eptWatchList[index].GroupId == ignored_group) ||
                !ept_watch_entry_is_effective(eptWatchList[index]) ||
                GET_PFN(eptWatchList[index].PhysicalAddress) != pfn)
            {
                continue;
            }

            if (eptWatchList[index].bpType == 3)
            {
                hooked_page_info->entry_address->execute = 0;
                has_effective_int3 = true;
                continue;
            }

            set_ept_watch(hooked_page_info, eptWatchList[index].Type);
        }

        // 隐藏 INT3 页必须连写权限一起收紧。伪页只是安装时刻的一份快照，
        // 而 entry 在隐藏态停在原页（read=1 / write=1 / execute=0），
        // guest 对原页的写入会静默成功，VMM 无从得知伪页已经过期 ——
        // 下一次执行该页时 ept_handle_int3_execute 换入的就是过期快照，
        // guest 会按旧指令流执行（表现为单步几步后跳回断点、步进位置与源码不符）。
        // 关掉 write 位后任何写入都会触发 EPT violation，由 ept_handleWatchEvent 置
        // fake_page_needs_full_sync，MTF 收尾时整页重新同步伪页。
        if (has_effective_int3)
        {
            hooked_page_info->entry_address->write = 0;
        }

        // changed_entry 同时是「本页存在 INT3 watch」的标记：vmexit_ept_violation_handler
        // 用 changed_entry.page_frame_number != 0 决定走 INT3 分支还是普通 EXECUTE watch 分支。
        // 页上最后一个 INT3 watch 消失后必须清掉，否则后续执行违规仍进 INT3 分支，
        // 而该分支在页上没有 INT3 watch 时只能返回 false，调用方走 unhandled 路径后
        // guest 会重试同一条指令并再次进入同一分支 —— 无进展自旋（0x101 / vmx_fatal_error）。
        if (!has_effective_int3)
        {
            hooked_page_info->changed_entry.page_frame_number = 0;
        }
    }

    bool ept_watch_activate_internal(
        __ept_state& ept_state,
        unsigned __int64 physical_address,
        unsigned __int64 Type)
    {
        if (physical_address == NULL)
        {
            LogError("请求的虚拟内存在物理内存中不存在");
            return false;
        }

        __ept_hooked_page_info* existing_hooked_page_info =
            ept::acquire_page_hot_reference(
                ept_state,
                GET_PFN(physical_address),
                EPTO_VIRTUAL_BREAKPOINT);
        if (existing_hooked_page_info != nullptr)
        {
            set_ept_watch(existing_hooked_page_info, Type);
            ept::release_page_hot_reference(ept_state, existing_hooked_page_info);
            return true;
        }

        __ept_dynamic_split* runtime_split = nullptr;
        if (!prepare_pml2_for_address(ept_state, physical_address))
        {
            LogError("Failed to prepare PML2 for watch page");
            return false;
        }

        if (!is_page_splitted(ept_state, physical_address))
        {
            void* split_buffer = pool_manager::request_pool<void*>(
                pool_manager::INTENTION_SPLIT_PML2,
                true,
                sizeof(__ept_dynamic_split));
            if (split_buffer == nullptr)
            {
                LogError("There is no preallocated pool for split");
                return false;
            }

            const auto mtrrs = hv::read_mtrr_data();
            if (!split_pml2(mtrrs, ept_state, split_buffer, physical_address))
            {
                pool_manager::release_pool(split_buffer);
                LogError("Split failed");
                return false;
            }

            ept_mark_runtime_split(split_buffer);
            runtime_split = reinterpret_cast<__ept_dynamic_split*>(split_buffer);
        }

        __ept_pte* target_page = get_pml1_entry(ept_state, physical_address);
        if (target_page == nullptr)
        {
            if (runtime_split != nullptr)
            {
                ept_split_try_merge_and_release(ept_state, runtime_split);
            }

            LogError("Failed to get PML1 entry of the target address");
            return false;
        }

        __ept_hooked_page_info* hooked_page_info =
            pool_manager::request_pool<__ept_hooked_page_info*>(
                pool_manager::INTENTION_TRACK_HOOKED_PAGES,
                true,
                sizeof(__ept_hooked_page_info));
        if (hooked_page_info == nullptr)
        {
            if (runtime_split != nullptr)
            {
                ept_split_try_merge_and_release(ept_state, runtime_split);
            }

            LogError("There is no preallocated pool for hooked page info");
            return false;
        }

        InitializeListHead(&hooked_page_info->hooked_functions_list);
        initialize_hooked_page_runtime_fields(hooked_page_info);
        hooked_page_info->pfn_of_hooked_page = GET_PFN(physical_address);
        hooked_page_info->entry_address = target_page;
        hooked_page_info->owner_split = ept_get_split_from_pml1(target_page);
        set_ept_watch(hooked_page_info, Type);
        ept_split_ref_inc(hooked_page_info->owner_split);
        ept::insert_hooked_page(ept_state, hooked_page_info);
        return true;
    }
}
