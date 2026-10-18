#include "PoolManagerInternal.h"

namespace pool_manager
{
    namespace
    {
        VOID PoolRefillWorker(_In_ PVOID StartContext)
        {
            __pool_manager* const manager =
                static_cast<__pool_manager*>(StartContext);

            for (;;)
            {
                LARGE_INTEGER timeout;
                timeout.QuadPart = -100000; // 10 ms：发现 root 侧无事件提交的补池请求
                KeWaitForSingleObject(
                    &manager->refill_event,
                    Executive,
                    KernelMode,
                    FALSE,
                    &timeout);

                if (InterlockedCompareExchange(
                    &manager->refill_stop,
                    1,
                    1) != 0)
                {
                    break;
                }

                (void)perform_allocation();
            }

            PsTerminateSystemThread(STATUS_SUCCESS);
        }

        bool start_refill_worker(__pool_manager* manager)
        {
            HANDLE thread_handle = nullptr;
            NTSTATUS status = PsCreateSystemThread(
                &thread_handle,
                THREAD_ALL_ACCESS,
                nullptr,
                nullptr,
                nullptr,
                PoolRefillWorker,
                manager);
            if (!NT_SUCCESS(status))
            {
                return false;
            }

            status = ObReferenceObjectByHandle(
                thread_handle,
                THREAD_ALL_ACCESS,
                *PsThreadType,
                KernelMode,
                reinterpret_cast<PVOID*>(&manager->refill_thread),
                nullptr);
            if (!NT_SUCCESS(status))
            {
                InterlockedExchange(&manager->refill_stop, 1);
                KeSetEvent(&manager->refill_event, IO_NO_INCREMENT, FALSE);
                (void)ZwWaitForSingleObject(thread_handle, FALSE, nullptr);
            }

            ZwClose(thread_handle);
            return NT_SUCCESS(status);
        }

        void stop_refill_worker(__pool_manager* manager)
        {
            if (manager->refill_thread == nullptr)
            {
                return;
            }

            InterlockedExchange(&manager->refill_stop, 1);
            KeSetEvent(&manager->refill_event, IO_NO_INCREMENT, FALSE);
            KeWaitForSingleObject(
                manager->refill_thread,
                Executive,
                KernelMode,
                FALSE,
                nullptr);
            ObDereferenceObject(manager->refill_thread);
            manager->refill_thread = nullptr;
        }
    }

    /// Initalize pool manager struct and preallocate pools
    /// 初始化池管理器结构并预分配池
    /// </summary>
    /// <returns> status </returns>
    /// <summary>
    bool initialize()
    {
        g_vmm_context.pool_manager = ::allocate_pool<__pool_manager>();
        if (g_vmm_context.pool_manager == nullptr)
        {
            LogError("Pool manager allocation failed");
            return false;
        }
        RtlSecureZeroMemory(g_vmm_context.pool_manager, sizeof(__pool_manager));
        KeInitializeSpinLock(
            &g_vmm_context.pool_manager->lock_for_reading_pool);
        KeInitializeEvent(
            &g_vmm_context.pool_manager->refill_event,
            SynchronizationEvent,
            FALSE);

        g_vmm_context.pool_manager->allocation_requests = ::allocate_pool<__request_new_allocation>();
        if (g_vmm_context.pool_manager->allocation_requests == nullptr)
        {
            LogError("Allacation requests allocation failed");
            goto failure;
        }
        RtlSecureZeroMemory(g_vmm_context.pool_manager->allocation_requests, sizeof(__request_new_allocation));

        g_vmm_context.pool_manager->list_of_allocated_pools = ::allocate_pool<LIST_ENTRY>();
        if (g_vmm_context.pool_manager->list_of_allocated_pools == nullptr)
        {
            LogError("List of allocated pools allocation failed");
            goto failure;
        }
        RtlSecureZeroMemory(g_vmm_context.pool_manager->list_of_allocated_pools, sizeof(LIST_ENTRY));

        InitializeListHead(g_vmm_context.pool_manager->list_of_allocated_pools);

        const unsigned __int32 buffer_count =
            static_cast<unsigned __int32>(g_vmm_context.processor_count) * 100u;

        if (request_allocation(sizeof(__ept_dynamic_split), buffer_count, INTENTION_SPLIT_PML2) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            goto failure;
        }

        if (request_allocation(sizeof(__ept_hooked_page_info), buffer_count, INTENTION_TRACK_HOOKED_PAGES) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            goto failure;
        }

        if (request_allocation(100, buffer_count, INTENTION_EXEC_TRAMPOLINE) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            goto failure;
        }

        if (request_allocation(sizeof(__ept_hooked_function_info), buffer_count, INTENTION_TRACK_HOOKED_FUNCTIONS) == false)
        {
            LogError("Pool mangaer request allocation Failed");
            goto failure;
        }

        if (perform_allocation() &&
            start_refill_worker(g_vmm_context.pool_manager))
        {
            return true;
        }

        LogError("Pool manager allocation/refill worker initialization failed");

failure:
        if (g_vmm_context.pool_manager != nullptr)
        {
            uninitialize();
            ::free_pool(g_vmm_context.pool_manager);
            g_vmm_context.pool_manager = nullptr;
        }
        return false;
    }

    /// <summary>
    /// Free all allocted pools
    /// 释放所有已分配的池
    /// </summary>
    void uninitialize()
    {
        PLIST_ENTRY ListHead, NextEntry, DelEntry;

        if (g_vmm_context.pool_manager == nullptr)
        {
            return;
        }

        stop_refill_worker(g_vmm_context.pool_manager);

        if (g_vmm_context.pool_manager->list_of_allocated_pools != nullptr)
        {
            ListHead = g_vmm_context.pool_manager->list_of_allocated_pools;
            NextEntry = ListHead->Flink;

            while (ListHead != NextEntry)
            {
                // Get the head of the record
                __pool_table* pool_table = (__pool_table*)CONTAINING_RECORD(NextEntry, __pool_table, pool_list);

                // Free the alloocated buffer
                if (pool_table->physically_aligned)
                {
                    ::free_vmm_page_aligned(pool_table->address);
                }
                else
                {
                    ::free_pool(pool_table->address);
                }

                DelEntry = NextEntry;
                // /* Move to the next entry */
                NextEntry = NextEntry->Flink;

                RemoveEntryList(DelEntry);
                // Free the record itself
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
