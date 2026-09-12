#include "PoolManagerInternal.h"

namespace pool_manager
{
    bool requires_physical_page_alignment(allocation_intention intention)
    {
        return intention == INTENTION_SPLIT_PML2 ||
            intention == INTENTION_TRACK_HOOKED_PAGES;
    }

    namespace
    {
        PLIST_ENTRY detach_pool_batch(PLIST_ENTRY list_head, PLIST_ENTRY first_inserted)
        {
            if (list_head == nullptr || first_inserted == nullptr)
            {
                return nullptr;
            }

            // allocate_pool_entries() 每次把新节点追加在链表尾部,因此被隔离的一批
            // 节点从 first_inserted 延续到链表尾。断开这段后再置空 Flink,后续释放
            // 就能按普通单向链表遍历,不会又绕回链表头。
            PLIST_ENTRY last_inserted = list_head->Blink;
            PLIST_ENTRY previous = first_inserted->Blink;

            previous->Flink = list_head;
            list_head->Blink = previous;
            last_inserted->Flink = nullptr;

            return first_inserted;
        }

        void free_pool_batch(PLIST_ENTRY first)
        {
            while (first != nullptr)
            {
                __pool_table* pool_table =
                    CONTAINING_RECORD(first, __pool_table, pool_list);
                PLIST_ENTRY next = first->Flink;

                if (pool_table->physically_aligned)
                {
                    ::free_vmm_page_aligned(pool_table->address);
                }
                else if (pool_table->address != nullptr)
                {
                    ::free_pool(pool_table->address);
                }
                ::free_pool(pool_table);

                first = next;
            }
        }
    }

    bool allocate_pool_entries(unsigned __int64 size, unsigned __int32 count, allocation_intention intention)
    {
        if (size == 0 || count == 0 || g_vmm_context.pool_manager == nullptr ||
            g_vmm_context.pool_manager->list_of_allocated_pools == nullptr)
        {
            LogError("Pool allocation request is invalid");
            return false;
        }

        PLIST_ENTRY list_head = g_vmm_context.pool_manager->list_of_allocated_pools;
        PLIST_ENTRY first_inserted = nullptr;

        spinlock::lock(&g_vmm_context.pool_manager->lock_for_reading_pool);
        for (unsigned int i = 0; i < count; i++)
        {
            __pool_table* single_pool = ::allocate_pool<__pool_table>();
            if (single_pool == nullptr)
            {
                LogError("Memory allocation failed");
                PLIST_ENTRY detached = detach_pool_batch(list_head, first_inserted);
                spinlock::unlock(&g_vmm_context.pool_manager->lock_for_reading_pool);
                free_pool_batch(detached);
                return false;
            }
            RtlSecureZeroMemory(single_pool, sizeof(__pool_table));

            single_pool->address = requires_physical_page_alignment(intention)
                ? ::allocate_page_aligned_pool<void*>(size)
                : ::allocate_pool<void*>(size);

            if (single_pool->address == nullptr)
            {
                LogError("Memory allocation failed");
                PLIST_ENTRY detached = detach_pool_batch(list_head, first_inserted);
                spinlock::unlock(&g_vmm_context.pool_manager->lock_for_reading_pool);
                free_pool_batch(detached);
                ::free_pool(single_pool);
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
                    PLIST_ENTRY detached = detach_pool_batch(list_head, first_inserted);
                    spinlock::unlock(&g_vmm_context.pool_manager->lock_for_reading_pool);
                    free_pool_batch(detached);
                    ::free_vmm_page_aligned(single_pool->address);
                    ::free_pool(single_pool);
                    return false;
                }
            }

            InsertTailList(g_vmm_context.pool_manager->list_of_allocated_pools, &(single_pool->pool_list));
            if (first_inserted == nullptr)
            {
                first_inserted = &single_pool->pool_list;
            }
        }
        spinlock::unlock(&g_vmm_context.pool_manager->lock_for_reading_pool);

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

        spinlock::lock(&g_vmm_context.pool_manager->lock_for_request_allocation);
        if (g_vmm_context.pool_manager->is_request_for_allocation_recived == false)
        {
            LogInfo("No pending allocations");
            spinlock::unlock(&g_vmm_context.pool_manager->lock_for_request_allocation);
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

        bool has_pending_allocation = false;
        for (unsigned __int64 i = 0; i < 10; ++i)
        {
            if (g_vmm_context.pool_manager->allocation_requests->size[i] != 0)
            {
                has_pending_allocation = true;
                break;
            }
        }
        g_vmm_context.pool_manager->is_request_for_allocation_recived = has_pending_allocation;
        spinlock::unlock(&g_vmm_context.pool_manager->lock_for_request_allocation);

        return status;
    }
}
