#include "PoolManagerInternal.h"

namespace pool_manager
{
    bool request_allocation(unsigned __int64 size, unsigned __int32 count, allocation_intention intention)
    {
        spinlock::lock(&g_vmm_context.pool_manager->lock_for_request_allocation);

        for (unsigned __int64 i = 0; i < 10; i++)
        {
            if (g_vmm_context.pool_manager->allocation_requests->size[i] == 0)
            {
                g_vmm_context.pool_manager->allocation_requests->count[i] = count;
                g_vmm_context.pool_manager->allocation_requests->size[i] = size;
                g_vmm_context.pool_manager->allocation_requests->intention[i] = intention;
                g_vmm_context.pool_manager->is_request_for_allocation_recived = true;
                break;
            }
        }

        spinlock::unlock(&g_vmm_context.pool_manager->lock_for_request_allocation);
        return g_vmm_context.pool_manager->is_request_for_allocation_recived;
    }

    void release_pool(void* address)
    {
        PLIST_ENTRY current = 0;
        current = g_vmm_context.pool_manager->list_of_allocated_pools;

        spinlock::lock(&g_vmm_context.pool_manager->lock_for_reading_pool);
        while (g_vmm_context.pool_manager->list_of_allocated_pools != current->Flink)
        {
            current = current->Flink;

            __pool_table* pool_table = (__pool_table*)CONTAINING_RECORD(current, __pool_table, pool_list);

            if (address == pool_table->address)
            {
                RtlSecureZeroMemory(address, pool_table->size);
                pool_table->is_busy = false;
                pool_table->recycled = true;
                break;
            }
        }

        spinlock::unlock(&g_vmm_context.pool_manager->lock_for_reading_pool);
    }
}
