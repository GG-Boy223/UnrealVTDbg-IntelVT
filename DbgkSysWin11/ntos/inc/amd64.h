#pragma once

#ifndef _AMD64_H
#define _AMD64_H

// AMD64 Feature bit definitions
//
// Kernel Feature Bits
//
#define KF_V86_VIS                      0x00000001
#define KF_RDTSC                        0x00000002
#define KF_CR4                          0x00000004
#define KF_CMOV                         0x00000008
#define KF_GLOBAL_PAGE                  0x00000010
#define KF_LARGE_PAGE                   0x00000020
#define KF_MTRR                         0x00000040
#define KF_CMPXCHG8B                    0x00000080
#define KF_MMX                          0x00000100
#define KF_WORKING_PTE                  0x00000200
#define KF_PAT                          0x00000400
#define KF_FXSR                         0x00000800
#define KF_FAST_SYSCALL                 0x00001000
#define KF_XMMI                         0x00002000
#define KF_3DNOW                        0x00004000
#define KF_AMDK6MTRR                    0x00008000
#define KF_XMMI64                       0x00010000
#define KF_DTS                          0x00020000
#define KF_BRANCH                       0x00020000 // from ksamd64.inc
#define KF_SSE3                         0x00080000
#define KF_CMPXCHG16B                   0x00100000
#define KF_XSTATE                       0x00800000 // from ks386.inc, ksamd64.inc
#define KF_NX_BIT                       0x20000000
#define KF_NX_DISABLED                  0x40000000
#define KF_NX_ENABLED                   0x80000000

#define KF_XSAVEOPT_BIT                 15
#define KF_XSTATE_BIT                   23
#define KF_RDWRFSGSBASE_BIT             28

#define KF_XSTATEFEATURES              3

//
// Define EFLAG bit masks and shift offsets.
//
#define EFLAGS_AC_MASK 0x00040000       // alignment check

//
// Define EFLAG bit masks and shift offsets.
//
#define EFLAGS_CF_MASK 0x00000001       // carry flag
#define EFLAGS_PF_MASK 0x00000004       // parity flag
#define EFLAGS_AF_MASK 0x00000010       // auxiliary carry flag
#define EFLAGS_ZF_MASK 0x00000040       // zero flag
#define EFLAGS_SF_MASK 0x00000080       // sign flag
#define EFLAGS_TF_MASK 0x00000100       // trap flag
#define EFLAGS_IF_MASK 0x00000200       // interrupt flag
#define EFLAGS_DF_MASK 0x00000400       // direction flag
#define EFLAGS_OF_MASK 0x00000800       // overflow flag
#define EFLAGS_IOPL_MASK 0x00003000     // I/O privilege level
#define EFLAGS_NT_MASK 0x00004000       // nested task
#define EFLAGS_RF_MASK 0x00010000       // resume flag
#define EFLAGS_VM_MASK 0x00020000       // virtual 8086 mode
#define EFLAGS_AC_MASK 0x00040000       // alignment check
#define EFLAGS_VIF_MASK 0x00080000      // virtual interrupt flag
#define EFLAGS_VIP_MASK 0x00100000      // virtual interrupt pending
#define EFLAGS_ID_MASK 0x00200000       // identification flag

// begin_wx86 begin_nthal
//
//  GDT selector numbers.
//
// N.B. There is code in context swap that "cleanses" the user segment
//      registers ds, es, fs, and gs. If these values are changed or
//      added to, then it is very likely the code in context swap will
//      have to be change.
//
#define KGDT64_NULL (0 * 16)            // NULL descriptor
#define KGDT64_R0_CODE (1 * 16)         // kernel mode 64-bit code
#define KGDT64_R0_DATA (1 * 16) + 8     // kernel mode 64-bit data (stack)
#define KGDT64_R3_CMCODE (2 * 16)       // user mode 32-bit code
#define KGDT64_R3_DATA (2 * 16) + 8     // user mode 32-bit data
#define KGDT64_R3_CODE (3 * 16)         // user mode 64-bit code
#define KGDT64_SYS_TSS (4 * 16)         // kernel mode system task state
#define KGDT64_R3_CMTEB (5 * 16)        // user mode 32-bit TEB
#define KGDT64_R0_CMCODE (6 * 16)       // kernel mode 32-bit code
#define KGDT64_LAST (7 * 16)            // last entry

#define ReadForWriteAccess(p) (_m_prefetchw(p), *(p))

// processor idle functions

typedef struct {
    ULONGLONG                   StartTime;
    ULONGLONG                   EndTime;
    ULONG                       IdleHandlerReserved[4];
} PROCESSOR_IDLE_TIMES, * PPROCESSOR_IDLE_TIMES;

typedef struct {
    UCHAR                       PercentFrequency;   // max == POWER_PERF_SCALE
    UCHAR                       MinCapacity;        // battery capacity %
    USHORT                      Power;              // in milliwatts
    UCHAR                       IncreaseLevel;      // goto higher state
    UCHAR                       DecreaseLevel;      // goto lower state
    USHORT                      Flags;
    ULONG                       IncreaseTime;       // in tick counts
    ULONG                       DecreaseTime;       // in tick counts
    ULONG                       IncreaseCount;      // goto higher state
    ULONG                       DecreaseCount;      // goto lower state
    ULONGLONG                   PerformanceTime;    // Tick count
} PROCESSOR_PERF_STATE, * PPROCESSOR_PERF_STATE;


//
// Power structure in each processors PRCB
//
struct _PROCESSOR_POWER_STATE;      // forward ref

typedef
VOID
(FASTCALL* PPROCESSOR_IDLE_FUNCTION) (
    struct _PROCESSOR_POWER_STATE* PState
    );

typedef
NTSTATUS
(FASTCALL* PSET_PROCESSOR_THROTTLE2) (
    IN UCHAR                    Throttle
    );

//
// Inline function to get current KPRCB
//
//PKPRCB FORCEINLINE KeGetCurrentPrcb(VOID)
//{
//    return (PKPRCB)__readgsqword(FIELD_OFFSET(KPCR, CurrentPrcb));
//}

#endif // !_AMD64_H
