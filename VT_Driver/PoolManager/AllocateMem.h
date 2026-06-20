#pragma once

#ifndef _ALLOCATEMEM_H
#define _ALLOCATEMEM_H

inline PVOID allocate_vmm_pool(unsigned __int64 size)
{
    if (size == 0)
    {
        return nullptr;
    }

#pragma warning(suppress:4996)
    return ExAllocatePoolWithTag(NonPagedPool, size, VMM_TAG);
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
        return nullptr;
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
