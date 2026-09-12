#pragma once

#ifndef _ALLOCATEMEM_H
#define _ALLOCATEMEM_H

inline PVOID allocate_vmm_pool(unsigned __int64 size)
{
    if (size == 0)
    {
        return nullptr; // 失败返回空指针
    }

#pragma warning(suppress:4996)
    return ExAllocatePoolWithTag(NonPagedPool, size, VMM_TAG);
}

inline PVOID allocate_vmm_page_aligned(unsigned __int64 size)
{
    if (size == 0)
    {
        return nullptr;
    }

    size = (size + PAGE_SIZE - 1) & ~(static_cast<unsigned __int64>(PAGE_SIZE) - 1);

    PHYSICAL_ADDRESS highest_address;
    highest_address.QuadPart = MAXULONG64;

    // EPT/IO bitmap/VMX 等硬件结构要求 4KB 物理对齐;普通池分配只保证虚拟对齐,
    // 不保证 MmGetPhysicalAddress 后的 4KB 对齐,因此这些页面必须走连续物理内存分配。
    return MmAllocateContiguousMemory(size, highest_address);
}

inline void free_vmm_page_aligned(void* pool_address)
{
    if (pool_address != nullptr)
    {
        MmFreeContiguousMemory(pool_address);
    }
}

template <typename T>
inline T allocate_pool(unsigned __int64 size)
{
    return static_cast<T>(allocate_vmm_pool(size));
}

template <typename T>
inline T* allocate_pool()
{
    return static_cast<T*>(allocate_vmm_pool(sizeof(T)));
}

template <typename T>
inline T allocate_page_aligned_pool(unsigned __int64 size)
{
    return static_cast<T>(allocate_vmm_page_aligned(size));
}

template <typename T>
inline T* allocate_page_aligned_pool()
{
    return static_cast<T*>(allocate_vmm_page_aligned(sizeof(T)));
}

template <typename T>
inline T* allocate_contignous_memory()
{
    PHYSICAL_ADDRESS a;
    a.QuadPart = 0ULL - 1;
    return static_cast<T*>(MmAllocateContiguousMemory(sizeof(T), a));
}

template <typename T>
inline T allocate_contignous_memory(unsigned __int64 size)
{
    if (size == 0)
    {
        return nullptr; // 失败返回空指针
    }

    PHYSICAL_ADDRESS a;
    a.QuadPart = 0ULL - 1;
    return static_cast<T>(MmAllocateContiguousMemory(size, a));
}

inline void free_pool(void* pool_address)
{
    if (pool_address)
    {
        ExFreePoolWithTag(pool_address, VMM_TAG);
    }
}

//释放连续内存
inline void free_contignous_memory(void* memory_address)
{
    if (memory_address)
    {
        MmFreeContiguousMemory(memory_address);
    }
}

#endif // !_ALLOCATEMEM_H
