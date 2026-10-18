#include "PoolManagerInternal.h"

namespace pool_manager
{
    inline const char* intention_to_string(allocation_intention intention)
    {
        switch (intention)
        {
        case INTENTION_NONE:   return "None";
        case INTENTION_TRACK_HOOKED_PAGES:   return "Track Hooked Pages";
        case INTENTION_EXEC_TRAMPOLINE: return "Trampoline";
        case INTENTION_SPLIT_PML2: return "Split Pml2";
        case INTENTION_TRACK_HOOKED_FUNCTIONS: return "Trace Hooked Functions";
        default:      return "Unknown";
        }
    }

    /// <summary>
    /// Writes all information about allocated pools
    /// </summary>
    void dump_pools_info()
    {
        PLIST_ENTRY current = g_vmm_context.pool_manager->list_of_allocated_pools;

        KIRQL old_irql;
        KeAcquireSpinLock(
            &g_vmm_context.pool_manager->lock_for_reading_pool,
            &old_irql);

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "-----------------------------------POOL MANAGER DUMP-----------------------------------\r\n");

        while (g_vmm_context.pool_manager->list_of_allocated_pools != current->Flink)
        {
            current = current->Flink;

            // Get the head of the record
            __pool_table* pool_table = (__pool_table*)CONTAINING_RECORD(current, __pool_table, pool_list);

            LogDump("Address: 0x%X    Size: %llu    Intention: %s    Is Busy: %s    Recycled: %s",
                pool_table->address, pool_table->size, intention_to_string(pool_table->intention), pool_table->is_busy ? "Yes" : "No",
                pool_table->recycled ? "Yes" : "No");
        }

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "-----------------------------------POOL MANAGER DUMP-----------------------------------\r\n");

        KeReleaseSpinLock(
            &g_vmm_context.pool_manager->lock_for_reading_pool,
            old_irql);
    }
}
