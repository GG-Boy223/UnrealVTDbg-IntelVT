#include "PoolManagerInternal.h"

namespace pool_manager
{
    namespace
    {
        volatile LONG g_allocation_in_progress = 0;

        void free_pool_batch(PLIST_ENTRY list_head)
        {
            while (!IsListEmpty(list_head))
            {
                PLIST_ENTRY entry = RemoveHeadList(list_head);
                __pool_table* pool_table =
                    CONTAINING_RECORD(entry, __pool_table, pool_list);

                if (pool_table->physically_aligned)
                {
                    ::free_vmm_page_aligned(pool_table->address);
                }
                else if (pool_table->address != nullptr)
                {
                    ::free_pool(pool_table->address);
                }
                ::free_pool(pool_table);
            }
        }

    }

    bool requires_physical_page_alignment(allocation_intention intention)
    {
        return intention == INTENTION_SPLIT_PML2 ||
            intention == INTENTION_TRACK_HOOKED_PAGES;
    }

    bool allocate_pool_entries(unsigned __int64 size, unsigned __int32 count, allocation_intention intention)
    {
        if (size == 0 || count == 0 || g_vmm_context.pool_manager == nullptr ||
            g_vmm_context.pool_manager->list_of_allocated_pools == nullptr)
        {
            LogError("Pool allocation request is invalid");
            return false;
        }

        LIST_ENTRY local_pools;
        InitializeListHead(&local_pools);

        for (unsigned int i = 0; i < count; i++)
        {
            __pool_table* single_pool = ::allocate_pool<__pool_table>();
            if (single_pool == nullptr)
            {
                LogError("Memory allocation failed");
                free_pool_batch(&local_pools);
                return false;
            }
            RtlSecureZeroMemory(single_pool, sizeof(__pool_table));

            single_pool->address = requires_physical_page_alignment(intention)
                ? ::allocate_page_aligned_pool<void*>(size)
                : ::allocate_pool<void*>(size);

            if (single_pool->address == nullptr)
            {
                LogError("Memory allocation failed");
                ::free_pool(single_pool);
                free_pool_batch(&local_pools);
                return false;
            }
            RtlSecureZeroMemory(single_pool->address, size);

            single_pool->physically_aligned = requires_physical_page_alignment(intention);
            single_pool->intention = intention;
            single_pool->is_busy = false; //将内存标识为未使用
            single_pool->size = size;

            if (single_pool->physically_aligned)
            {
                unsigned __int64 const physical_address =
                    MmGetPhysicalAddress(single_pool->address).QuadPart;
                if ((physical_address & (PAGE_SIZE - 1)) != 0)
                {
                    LogError("Physically aligned pool allocation returned unaligned memory");
                    ::free_vmm_page_aligned(single_pool->address);
                    ::free_pool(single_pool);
                    free_pool_batch(&local_pools);
                    return false;
                }
            }

            InsertTailList(&local_pools, &(single_pool->pool_list));
        }

        // ExAllocatePool / MmAllocateContiguousMemory 可能阻塞，绝不能持有全局池锁执行。
        // 先在本地链构造完整批次，最后只在锁内发布节点；失败时本地批次整体释放。
        KIRQL old_irql;
        KeAcquireSpinLock(
            &g_vmm_context.pool_manager->lock_for_reading_pool,
            &old_irql);
        while (!IsListEmpty(&local_pools))
        {
            PLIST_ENTRY entry = RemoveHeadList(&local_pools);
            InsertTailList(
                g_vmm_context.pool_manager->list_of_allocated_pools,
                entry);
        }
        KeReleaseSpinLock(
            &g_vmm_context.pool_manager->lock_for_reading_pool,
            old_irql);

        return true;
    }

    /// <summary>
    /// 分配所有请求的池
    /// </summary>
    /// <returns></returns>
    bool perform_allocation()
    {
        bool status = true;

        if (g_vmm_context.pool_manager == nullptr ||
            g_vmm_context.pool_manager->allocation_requests == nullptr)
        {
            LogError("Pool manager is not initialized");
            return false;
        }

        if (KeGetCurrentIrql() != PASSIVE_LEVEL ||
            is_current_context_vmx_root())
        {
            return false;
        }

        if (InterlockedCompareExchange(
            &g_allocation_in_progress,
            1,
            0) != 0)
        {
            return false;
        }

        for (;;)
        {
            __request_new_allocation request = {};
            int request_index = -1;

            for (unsigned __int64 i = 0; i < 10; ++i)
            {
                if (InterlockedCompareExchange(
                    &g_vmm_context.pool_manager->allocation_requests->state[i],
                    REQUEST_SLOT_ALLOCATING,
                    REQUEST_SLOT_READY) == REQUEST_SLOT_READY)
                {
                    request.size[0] =
                        g_vmm_context.pool_manager->allocation_requests->size[i];
                    request.count[0] =
                        g_vmm_context.pool_manager->allocation_requests->count[i];
                    request.intention[0] =
                        g_vmm_context.pool_manager->allocation_requests->intention[i];
                    request_index = static_cast<int>(i);
                    break;
                }
            }

            if (request_index < 0)
            {
                break;
            }

            if (!allocate_pool_entries(
                request.size[0],
                request.count[0],
                request.intention[0]))
            {
                LogError("Pool manager allocate and add to pool table failed");
                status = false;
                g_vmm_context.pool_manager->allocation_requests->size[request_index] = 0;
                g_vmm_context.pool_manager->allocation_requests->count[request_index] = 0;
                g_vmm_context.pool_manager->allocation_requests->intention[request_index] =
                    INTENTION_NONE;
                InterlockedExchange(
                    &g_vmm_context.pool_manager->allocation_requests->state[request_index],
                    REQUEST_SLOT_FREE);
                break;
            }

            g_vmm_context.pool_manager->allocation_requests->size[request_index] = 0;
            g_vmm_context.pool_manager->allocation_requests->count[request_index] = 0;
            g_vmm_context.pool_manager->allocation_requests->intention[request_index] =
                INTENTION_NONE;
            InterlockedExchange(
                &g_vmm_context.pool_manager->allocation_requests->state[request_index],
                REQUEST_SLOT_FREE);
            LogInfo("Allocation successful");
        }

        bool pending = false;
        for (unsigned __int64 i = 0; i < 10; ++i)
        {
            if (g_vmm_context.pool_manager->allocation_requests->state[i] !=
                REQUEST_SLOT_FREE)
            {
                pending = true;
                break;
            }
        }
        InterlockedExchange(
            &g_vmm_context.pool_manager->is_request_for_allocation_recived,
            pending ? 1 : 0);

        InterlockedExchange(&g_allocation_in_progress, 0);
        return status;
    }
}
