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

        const unsigned __int64 pfn = hooked_page_info->pfn_of_hooked_page;
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
                continue;
            }

            set_ept_watch(hooked_page_info, eptWatchList[index].Type);
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

        PLIST_ENTRY current = &ept_state.hooked_page_list;
        while (&ept_state.hooked_page_list != current->Flink)
        {
            current = current->Flink;
            __ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(
                current,
                __ept_hooked_page_info,
                hooked_page_list);

            if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address) &&
                hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
            {
                set_ept_watch(hooked_page_info, Type);
                return true;
            }
        }

        __ept_dynamic_split* runtime_split = nullptr;
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
        InsertHeadList(
            &ept_state.hooked_page_list,
            &hooked_page_info->hooked_page_list);
        return true;
    }
}
