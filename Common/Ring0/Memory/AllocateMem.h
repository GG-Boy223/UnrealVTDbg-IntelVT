#pragma once

#ifndef _ALLOCATEMEM_H
#define _ALLOCATEMEM_H

#define NPPM_TAG 'nppm'

inline PVOID allocate_nonpaged_pool_zero(unsigned __int64 size)
{
    if (size == 0)
    {
        return nullptr;
    }

#pragma warning(suppress:4996)
    PVOID pAddr = ExAllocatePoolWithTag(NonPagedPool, size, NPPM_TAG);
    if (pAddr != nullptr)
    {
        RtlSecureZeroMemory(pAddr, size);
    }
    return pAddr;
}

template <typename T>
inline T allocate_pool(unsigned __int64 size)
{
    return static_cast<T>(allocate_nonpaged_pool_zero(size));
}

template <typename T>
inline T* allocate_pool()
{
    return static_cast<T*>(allocate_nonpaged_pool_zero(sizeof(T)));
}

template <typename T>
inline T* allocate_contignous_memory()
{
    PHYSICAL_ADDRESS a;
    a.QuadPart = 0ULL - 1;
    PVOID pAddr = MmAllocateContiguousMemory(sizeof(T), a);
    if (pAddr != nullptr)
    {
        RtlSecureZeroMemory(pAddr, sizeof(T));
    }
    return static_cast<T*>(pAddr);
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
    PVOID pAddr = MmAllocateContiguousMemory(size, a);
    if (pAddr != nullptr)
    {
        RtlSecureZeroMemory(pAddr, size);
    }
    return static_cast<T>(pAddr);
}

inline void free_pool(void* pool_address)
{
    if (pool_address)
    {
        ExFreePoolWithTag(pool_address, NPPM_TAG);
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
