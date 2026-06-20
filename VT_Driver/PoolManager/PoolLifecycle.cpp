#include "PoolManagerInternal.h"

namespace pool_manager
{
    bool initialize()
    {
        g_vmm_context.pool_manager = ::allocate_pool<__pool_manager>();
        if (g_vmm_context.pool_manager == nullptr)
        {
            LogError("Pool manager allocation failed");
            return false;
        }
        RtlSecureZeroMemory(g_vmm_context.pool_manager, sizeof(__pool_manager));

        g_vmm_context.pool_manager->allocation_requests = ::allocate_pool<__request_new_allocation>();
        if (g_vmm_context.pool_manager->allocation_requests == nullptr)
        {
            LogError("Allacation requests allocation failed");
            return false;
        }
        RtlSecureZeroMemory(g_vmm_context.pool_manager->allocation_requests, sizeof(__request_new_allocation));

        g_vmm_context.pool_manager->list_of_allocated_pools = ::allocate_pool<LIST_ENTRY>();
        if (g_vmm_context.pool_manager->list_of_allocated_pools == nullptr)
        {
            LogError("List of allocated pools allocation failed");
            return false;
        }
        RtlSecureZeroMemory(g_vmm_context.pool_manager->list_of_allocated_pools, sizeof(LIST_ENTRY));

        InitializeListHead(g_vmm_context.pool_manager->list_of_allocated_pools);

        const unsigned __int32 buffer_count =
            static_cast<unsigned __int32>(g_vmm_context.processor_count) * 100u;

        if (request_allocation(sizeof(__ept_dynamic_split), buffer_count, INTENTION_SPLIT_PML2) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            return false;
        }

        if (request_allocation(sizeof(__ept_hooked_page_info), buffer_count, INTENTION_TRACK_HOOKED_PAGES) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            return false;
        }

        if (request_allocation(100, buffer_count, INTENTION_EXEC_TRAMPOLINE) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            return false;
        }

        if (request_allocation(sizeof(__ept_hooked_function_info), buffer_count, INTENTION_TRACK_HOOKED_FUNCTIONS) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            return false;
        }

        return perform_allocation();
    }

    void uninitialize()
    {
        PLIST_ENTRY ListHead, NextEntry, DelEntry;

        if (g_vmm_context.pool_manager->list_of_allocated_pools != nullptr)
        {
            ListHead = g_vmm_context.pool_manager->list_of_allocated_pools;
            NextEntry = ListHead->Flink;

            while (ListHead != NextEntry)
            {
                __pool_table* pool_table = (__pool_table*)CONTAINING_RECORD(NextEntry, __pool_table, pool_list);

                free_pool(pool_table->address);

                DelEntry = NextEntry;
                NextEntry = NextEntry->Flink;

                RemoveEntryList(DelEntry);
                free_pool(pool_table);
            }

            free_pool(g_vmm_context.pool_manager->list_of_allocated_pools);
        }

        if (g_vmm_context.pool_manager->allocation_requests != nullptr)
        {
            free_pool(g_vmm_context.pool_manager->allocation_requests);
        }
    }
}
