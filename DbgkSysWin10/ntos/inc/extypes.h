#pragma once

#ifndef _EX_H
#define _EX_H

//
// Define a structure to track handle usage
//
#define HANDLE_TRACE_DB_MAX_STACKS 65536
#define HANDLE_TRACE_DB_MIN_STACKS 128
#define HANDLE_TRACE_DB_DEFAULT_STACKS 4096
#define HANDLE_TRACE_DB_STACK_SIZE 16


typedef struct _MY_ERESOURCE {
    LIST_ENTRY SystemResourcesList;
    POWNER_ENTRY OwnerTable;
    SHORT ActiveCount;
    USHORT Flag;
    PKSEMAPHORE SharedWaiters;
    PKEVENT ExclusiveWaiters;
    OWNER_ENTRY OwnerThreads[2];
    ULONG ContentionCount;
    USHORT NumberOfSharedWaiters;
    USHORT NumberOfExclusiveWaiters;
    union {
        PVOID Address;
        ULONG_PTR CreatorBackTraceIndex;
    };

    KSPIN_LOCK SpinLock;
} MY_ERESOURCE, * PMY_ERESOURCE;

//VOID
//FORCEINLINE 
//ExAcquirePushLockShared(IN PEX_PUSH_LOCK PushLock)
//{
//    EX_PUSH_LOCK OldValue, NewValue;
//    OldValue.Value = 0;
//    NewValue.Value = EX_PUSH_LOCK_SHARE_INC | EX_PUSH_LOCK_LOCK;
//
//    if (InterlockedCompareExchangePointer(&PushLock->Ptr, NewValue.Ptr, OldValue.Ptr) != OldValue.Ptr)
//    {
//        ExfAcquirePushLockShared((PULONG_PTR)PushLock);
//    }
//}

//VOID
//FORCEINLINE
//ExReleasePushLockShared(IN PEX_PUSH_LOCK PushLock)
//{
//    EX_PUSH_LOCK OldValue, NewValue;
//    OldValue.Value = EX_PUSH_LOCK_SHARE_INC | EX_PUSH_LOCK_LOCK;
//    NewValue.Value = 0;
//
//    if (InterlockedCompareExchangePointer(&PushLock->Ptr, NewValue.Ptr, OldValue.Ptr) != OldValue.Ptr)
//    {
//        ExfReleasePushLockShared((PULONG_PTR)PushLock);
//    }
//}

VOID
FORCEINLINE
ProbeForReadSmallStructure(
    IN PVOID Address,
    IN SIZE_T Size,
    IN ULONG Alignment
)

/*++

Routine Description:

    Probes a structure for read access whose size is known at compile time.

    N.B. A NULL structure address is not allowed.

Arguments:

    Address - Supples a pointer to the structure.

    Size - Supplies the size of the structure.

    Alignment - Supplies the alignment of structure.

Return Value:

    None

--*/

{

    ASSERT((Alignment == 1) || (Alignment == 2) ||
        (Alignment == 4) || (Alignment == 8) ||
        (Alignment == 16));

    if ((Size == 0) || (Size >= 0x10000)) {

        ASSERT(0);

        ProbeForRead(Address, Size, Alignment);

    }
    else {
        if (((ULONG_PTR)Address & (Alignment - 1)) != 0) {
            ExRaiseDatatypeMisalignment();
        }

        if ((PUCHAR)Address >= (UCHAR* const)MM_USER_PROBE_ADDRESS) {
            Address = (UCHAR* const)MM_USER_PROBE_ADDRESS;
        }

        _ReadWriteBarrier();
        *(volatile UCHAR*)Address;
    }
}


// begin_ntosp

VOID
FORCEINLINE
ProbeForWriteSmallStructure(
    IN PVOID Address,
    IN SIZE_T Size,
    IN ULONG Alignment
)

/*++

Routine Description:

    Probes a structure for write access whose size is known at compile time.

Arguments:

    Address - Supples a pointer to the structure.

    Size - Supplies the size of the structure.

    Alignment - Supplies the alignment of structure.

Return Value:

    None

--*/

{

    ASSERT((Alignment == 1) || (Alignment == 2) ||
        (Alignment == 4) || (Alignment == 8) ||
        (Alignment == 16));

    //
    // If the size of the structure is > 4k then call the standard routine.
    // wow64 uses a page size of 4k even on ia64.
    //

    if ((Size == 0) || (Size >= 0x1000)) {

        ASSERT(0);

        ProbeForWrite(Address, Size, Alignment);

    }
    else {
        if (((ULONG_PTR)(Address) & (Alignment - 1)) != 0) {
            ExRaiseDatatypeMisalignment();
        }

#if defined(_AMD64_)

        if ((ULONG_PTR)(Address) >= (ULONG_PTR)MM_USER_PROBE_ADDRESS) {
            Address = (UCHAR* const)MM_USER_PROBE_ADDRESS;
        }

        ((volatile UCHAR*)(Address))[0] = ((volatile UCHAR*)(Address))[0];
        ((volatile UCHAR*)(Address))[Size - 1] = ((volatile UCHAR*)(Address))[Size - 1];

#else

        if ((ULONG_PTR)(Address) >= (ULONG_PTR)MM_USER_PROBE_ADDRESS) {
            *((volatile UCHAR* const)MM_USER_PROBE_ADDRESS) = 0;
        }

        *(volatile UCHAR*)(Address) = *(volatile UCHAR*)(Address);
        if (Size > Alignment) {
            ((volatile UCHAR*)(Address))[(Size - 1) & ~(SIZE_T)(Alignment - 1)] =
                ((volatile UCHAR*)(Address))[(Size - 1) & ~(SIZE_T)(Alignment - 1)];
        }

#endif

    }
}


#endif // !_EX_H
