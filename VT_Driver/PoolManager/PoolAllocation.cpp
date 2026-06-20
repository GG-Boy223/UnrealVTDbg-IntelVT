#include "PoolManagerInternal.h"

namespace pool_manager
{
    bool allocate_pool_entries(unsigned __int64 size, unsigned __int32 count, allocation_intention intention)
    {
        for (unsigned int i = 0; i < count; i++)
        {
            __pool_table* single_pool = ::allocate_pool<__pool_table>();
            if (single_pool == nullptr)
            {
                LogError("Memory allocation failed");
                return false;
            }
            RtlSecureZeroMemory(single_pool, sizeof(__pool_table));

            single_pool->address = ::allocate_pool<void*>(size);

            if (single_pool->address == nullptr)
            {
                LogError("Memory allocation failed");
                return false;
            }
            RtlSecureZeroMemory(single_pool->address, size);

            single_pool->intention = intention;
            single_pool->is_busy = false;
            single_pool->size = size;

            InsertTailList(g_vmm_context.pool_manager->list_of_allocated_pools, &(single_pool->pool_list));
        }

        return true;
    }

    bool perform_allocation()
    {
        bool status = true;

        if (g_vmm_context.pool_manager->is_request_for_allocation_recived == false)
        {
            LogInfo("No pending allocations");
            return status;
        }

        for (unsigned __int64 i = 0; i < 10; i++)
        {
            if (g_vmm_context.pool_manager->allocation_requests->size[i] != 0)
            {
                status = allocate_pool_entries
                (
                    g_vmm_context.pool_manager->allocation_requests->size[i],
                    g_vmm_context.pool_manager->allocation_requests->count[i],
                    g_vmm_context.pool_manager->allocation_requests->intention[i]
                );

                if (status == false)
                {
                    LogError("Pool manager allocate and add to pool table failed");
                    break;
                }

                g_vmm_context.pool_manager->allocation_requests->size[i] = 0;
                g_vmm_context.pool_manager->allocation_requests->count[i] = 0;
                g_vmm_context.pool_manager->allocation_requests->intention[i] = INTENTION_NONE;

                LogInfo("Allocation successful");
            }
        }

        g_vmm_context.pool_manager->is_request_for_allocation_recived = false;

        return status;
    }
}
