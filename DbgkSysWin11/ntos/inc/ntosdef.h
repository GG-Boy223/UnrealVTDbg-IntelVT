/*该头文件定义的结构仅用于测试使用*/

#pragma once

#ifndef _NTOSDEF_H
#define _NTOSDEF_H

#define POOL_SMALL_LISTS 32

//typedef VOID(__fastcall* PKRUNDOWN_ROUTINE)(_In_ struct _MY_KAPC* Apc);


typedef struct _KREQUEST_PACKET
{
    /* 0x0000 */ void* CurrentPacket[3];
    /* 0x0018 */ void* WorkerRoutine /* function */;
} KREQUEST_PACKET, * PKREQUEST_PACKET; /* size: 0x0020 */

typedef struct _REQUEST_MAILBOX
{
    /* 0x0000 */ struct _REQUEST_MAILBOX* Next;
    /* 0x0008 */ unsigned __int64 RequestSummary;
    /* 0x0010 */ struct _KREQUEST_PACKET RequestPacket;
    /* 0x0030 */ volatile long* NodeTargetCountAddr;
    /* 0x0038 */ volatile long NodeTargetCount;
    /* 0x003c */ long __PADDING__[1];
} REQUEST_MAILBOX, * PREQUEST_MAILBOX; /* size: 0x0040 */

typedef struct _KDESCRIPTOR
{
    /* 0x0000 */ unsigned short Pad[3];
    /* 0x0006 */ unsigned short Limit;
    /* 0x0008 */ void* Base;
} KDESCRIPTOR, * PKDESCRIPTOR; /* size: 0x0010 */

typedef struct _KSPECIAL_REGISTERS
{
    /* 0x0000 */ unsigned __int64 Cr0;
    /* 0x0008 */ unsigned __int64 Cr2;
    /* 0x0010 */ unsigned __int64 Cr3;
    /* 0x0018 */ unsigned __int64 Cr4;
    /* 0x0020 */ unsigned __int64 KernelDr0;
    /* 0x0028 */ unsigned __int64 KernelDr1;
    /* 0x0030 */ unsigned __int64 KernelDr2;
    /* 0x0038 */ unsigned __int64 KernelDr3;
    /* 0x0040 */ unsigned __int64 KernelDr6;
    /* 0x0048 */ unsigned __int64 KernelDr7;
    /* 0x0050 */ struct _KDESCRIPTOR Gdtr;
    /* 0x0060 */ struct _KDESCRIPTOR Idtr;
    /* 0x0070 */ unsigned short Tr;
    /* 0x0072 */ unsigned short Ldtr;
    /* 0x0074 */ unsigned long MxCsr;
    /* 0x0078 */ unsigned __int64 DebugControl;
    /* 0x0080 */ unsigned __int64 LastBranchToRip;
    /* 0x0088 */ unsigned __int64 LastBranchFromRip;
    /* 0x0090 */ unsigned __int64 LastExceptionToRip;
    /* 0x0098 */ unsigned __int64 LastExceptionFromRip;
    /* 0x00a0 */ unsigned __int64 Cr8;
    /* 0x00a8 */ unsigned __int64 MsrGsBase;
    /* 0x00b0 */ unsigned __int64 MsrGsSwap;
    /* 0x00b8 */ unsigned __int64 MsrStar;
    /* 0x00c0 */ unsigned __int64 MsrLStar;
    /* 0x00c8 */ unsigned __int64 MsrCStar;
    /* 0x00d0 */ unsigned __int64 MsrSyscallMask;
    /* 0x00d8 */ unsigned __int64 Xcr0;
    /* 0x00e0 */ unsigned __int64 MsrFsBase;
    /* 0x00e8 */ unsigned __int64 SpecialPadding0;
} KSPECIAL_REGISTERS, * PKSPECIAL_REGISTERS; /* size: 0x00f0 */

typedef union _KPRCBFLAG
{
    union
    {
        /* 0x0000 */ volatile long PrcbFlags;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned long BamQosLevel : 8; /* bit position: 0 */
            /* 0x0000 */ unsigned long PendingQosUpdate : 2; /* bit position: 8 */
            /* 0x0000 */ unsigned long CacheIsolationEnabled : 1; /* bit position: 10 */
            /* 0x0000 */ unsigned long TracepointActive : 1; /* bit position: 11 */
            /* 0x0000 */ unsigned long PrcbFlagsReserved : 20; /* bit position: 12 */
        }; /* bitfield */
    }; /* size: 0x0004 */
} KPRCBFLAG, * PKPRCBFLAG; /* size: 0x0004 */

typedef struct _KPROCESSOR_STATE
{
    /* 0x0000 */ struct _KSPECIAL_REGISTERS SpecialRegisters;
    /* 0x00f0 */ struct _CONTEXT ContextFrame;
} KPROCESSOR_STATE, * PKPROCESSOR_STATE; /* size: 0x05c0 */

typedef struct _PP_LOOKASIDE_LIST
{
    /* 0x0000 */ struct _GENERAL_LOOKASIDE* P;
    /* 0x0008 */ struct _GENERAL_LOOKASIDE* L;
} PP_LOOKASIDE_LIST, * PPP_LOOKASIDE_LIST; /* size: 0x0010 */

typedef struct _KAFFINITY_EX
{
    /* 0x0000 */ unsigned short Count;
    /* 0x0002 */ unsigned short Size;
    /* 0x0004 */ unsigned long Reserved;
    /* 0x0008 */ unsigned __int64 Bitmap[20];
} KAFFINITY_EX, * PKAFFINITY_EX; /* size: 0x00a8 */

typedef struct _KSTATIC_AFFINITY_BLOCK
{
    union
    {
        /* 0x0000 */ struct _KAFFINITY_EX KeFlushTbAffinity;
        /* 0x0000 */ struct _KAFFINITY_EX KeFlushWbAffinity;
        /* 0x0000 */ struct _KAFFINITY_EX KeSyncContextAffinity;
    }; /* size: 0x00a8 */
    /* 0x00a8 */ struct _KAFFINITY_EX KeFlushTbDeepIdleAffinity;
    /* 0x0150 */ struct _KAFFINITY_EX KeIpiSendAffinity;
    /* 0x01f8 */ struct _KAFFINITY_EX KeIpiSendIpiSet;
} KSTATIC_AFFINITY_BLOCK, * PKSTATIC_AFFINITY_BLOCK; /* size: 0x02a0 */

typedef struct _KDPC_LIST
{
    /* 0x0000 */ struct _SINGLE_LIST_ENTRY ListHead;
    /* 0x0008 */ struct _SINGLE_LIST_ENTRY* LastEntry;
} KDPC_LIST, * PKDPC_LIST; /* size: 0x0010 */

typedef struct _KDPC_DATA
{
    /* 0x0000 */ struct _KDPC_LIST DpcList;
    /* 0x0010 */ unsigned __int64 DpcLock;
    /* 0x0018 */ volatile long DpcQueueDepth;
    /* 0x001c */ unsigned long DpcCount;
    /* 0x0020 */ struct _KDPC* volatile ActiveDpc;
} KDPC_DATA, * PKDPC_DATA; /* size: 0x0028 */

typedef struct _KTIMER_TABLE_ENTRY
{
    /* 0x0000 */ unsigned __int64 Lock;
    /* 0x0008 */ struct _LIST_ENTRY Entry;
    /* 0x0018 */ union _ULARGE_INTEGER Time;
} KTIMER_TABLE_ENTRY, * PKTIMER_TABLE_ENTRY; /* size: 0x0020 */

typedef struct _KTIMER_TABLE_STATE
{
    /* 0x0000 */ unsigned __int64 LastTimerExpiration[2];
    /* 0x0010 */ unsigned long LastTimerHand[2];
} KTIMER_TABLE_STATE, * PKTIMER_TABLE_STATE; /* size: 0x0018 */

typedef struct _KTIMER_TABLE
{
    /* 0x0000 */ struct _KTIMER* TimerExpiry[64];
    /* 0x0200 */ struct _KTIMER_TABLE_ENTRY TimerEntries[256][2];
    /* 0x4200 */ struct _KTIMER_TABLE_STATE TableState;
} KTIMER_TABLE, * PKTIMER_TABLE; /* size: 0x4218 */

typedef struct _RTL_RB_TREE
{
    /* 0x0000 */ struct _RTL_BALANCED_NODE* Root;
    union
    {
        /* 0x0008 */ unsigned char Encoded : 1; /* bit position: 0 */
        /* 0x0008 */ struct _RTL_BALANCED_NODE* Min;
    }; /* size: 0x0008 */
} RTL_RB_TREE, * PRTL_RB_TREE; /* size: 0x0010 */


typedef struct _PROC_PERF_CHECK_CONTEXT
{
    /* 0x0000 */ struct _PROC_PERF_DOMAIN* Domain;
    /* 0x0008 */ struct _PROC_PERF_CONSTRAINT* Constraint;
    /* 0x0010 */ struct _PROC_PERF_CHECK* PerfCheck;
    /* 0x0018 */ struct _PROC_PERF_LOAD* Load;
    /* 0x0020 */ struct _PROC_PERF_HISTORY* PerfHistory;
    /* 0x0028 */ unsigned long Utility;
    /* 0x002c */ unsigned long AffinitizedUtility;
    /* 0x0030 */ unsigned long MediaUtility;
    /* 0x0034 */ unsigned short LatestAffinitizedPercent;
    /* 0x0036 */ unsigned short AveragePerformancePercent;
    /* 0x0038 */ unsigned long RelativePerformance;
    /* 0x003c */ unsigned char NtProcessor;
    /* 0x003d */ char __PADDING__[3];
} PROC_PERF_CHECK_CONTEXT, * PPROC_PERF_CHECK_CONTEXT; /* size: 0x0040 */

typedef struct _PROC_IDLE_SNAP
{
    /* 0x0000 */ unsigned __int64 Time;
    /* 0x0008 */ unsigned __int64 Idle;
} PROC_IDLE_SNAP, * PPROC_IDLE_SNAP; /* size: 0x0010 */

typedef struct _PPM_FFH_THROTTLE_STATE_INFO
{
    /* 0x0000 */ unsigned char EnableLogging;
    /* 0x0001 */ char Padding_122[3];
    /* 0x0004 */ unsigned long MismatchCount;
    /* 0x0008 */ unsigned char Initialized;
    /* 0x0009 */ char Padding_123[7];
    /* 0x0010 */ unsigned __int64 LastValue;
    /* 0x0018 */ union _LARGE_INTEGER LastLogTickCount;
} PPM_FFH_THROTTLE_STATE_INFO, * PPPM_FFH_THROTTLE_STATE_INFO; /* size: 0x0020 */


typedef struct _PROC_FEEDBACK
{
    /* 0x0000 */ unsigned __int64 Lock;
    /* 0x0008 */ unsigned __int64 CyclesLast;
    /* 0x0010 */ unsigned __int64 CyclesActive;
    /* 0x0018 */ struct _PROC_FEEDBACK_COUNTER* Counters[2];
    /* 0x0028 */ unsigned __int64 LastUpdateTime;
    /* 0x0030 */ unsigned __int64 UnscaledTime;
    /* 0x0038 */ volatile __int64 UnaccountedTime;
    /* 0x0040 */ unsigned __int64 ScaledTime[2];
    /* 0x0050 */ unsigned __int64 UnaccountedKernelTime;
    /* 0x0058 */ unsigned __int64 PerformanceScaledKernelTime;
    /* 0x0060 */ unsigned long UserTimeLast;
    /* 0x0064 */ unsigned long KernelTimeLast;
    /* 0x0068 */ unsigned __int64 IdleGenerationNumberLast;
    /* 0x0070 */ unsigned __int64 HvActiveTimeLast;
    /* 0x0078 */ unsigned __int64 StallCyclesLast;
    /* 0x0080 */ unsigned __int64 StallTime;
    /* 0x0088 */ unsigned char KernelTimesIndex;
    /* 0x0089 */ unsigned char CounterDiscardsIdleTime;
    /* 0x008a */ char __PADDING__[6];
} PROC_FEEDBACK, * PPROC_FEEDBACK; /* size: 0x0090 */

typedef union _PPM_IDLE_SYNCHRONIZATION_STATE
{
    union
    {
        /* 0x0000 */ long AsLong;
        struct /* bitfield */
        {
            /* 0x0000 */ long RefCount : 24; /* bit position: 0 */
            /* 0x0000 */ unsigned long State : 8; /* bit position: 24 */
        }; /* bitfield */
    }; /* size: 0x0004 */
} PPM_IDLE_SYNCHRONIZATION_STATE, * PPPM_IDLE_SYNCHRONIZATION_STATE; /* size: 0x0004 */

typedef struct _PROC_IDLE_POLICY
{
    /* 0x0000 */ unsigned char PromotePercent;
    /* 0x0001 */ unsigned char DemotePercent;
    /* 0x0002 */ unsigned char PromotePercentBase;
    /* 0x0003 */ unsigned char DemotePercentBase;
    /* 0x0004 */ unsigned char AllowScaling;
    /* 0x0005 */ unsigned char ForceLightIdle;
} PROC_IDLE_POLICY, * PPROC_IDLE_POLICY; /* size: 0x0006 */

typedef struct _PROCESSOR_POWER_STATE
{
    /* 0x0000 */ struct _PPM_IDLE_STATES* IdleStates;
    /* 0x0008 */ struct _PROC_IDLE_ACCOUNTING* IdleAccounting;
    /* 0x0010 */ unsigned __int64 IdleTimeLast;
    /* 0x0018 */ unsigned __int64 IdleTimeTotal;
    /* 0x0020 */ volatile unsigned __int64 IdleTimeEntry;
    /* 0x0028 */ unsigned __int64 IdleTimeExpiration;
    /* 0x0030 */ unsigned char NonInterruptibleTransition;
    /* 0x0031 */ unsigned char PepWokenTransition;
    /* 0x0032 */ unsigned char HvTargetState;
    /* 0x0033 */ unsigned char SoftParked;
    /* 0x0034 */ unsigned long TargetIdleState;
    /* 0x0038 */ struct _PROC_IDLE_POLICY IdlePolicy;
    /* 0x003e */ char Padding_187[2];
    /* 0x0040 */ volatile union _PPM_IDLE_SYNCHRONIZATION_STATE Synchronization;
    /* 0x0044 */ long Padding_188;
    /* 0x0048 */ struct _PROC_FEEDBACK PerfFeedback;
    /* 0x00d8 */ enum _PROC_HYPERVISOR_STATE Hypervisor;
    /* 0x00dc */ unsigned long LastSysTime;
    /* 0x00e0 */ unsigned __int64 WmiDispatchPtr;
    /* 0x00e8 */ long WmiInterfaceEnabled;
    /* 0x00ec */ long Padding_189;
    /* 0x00f0 */ struct _PPM_FFH_THROTTLE_STATE_INFO FFHThrottleStateInfo;
    /* 0x0110 */ struct _KDPC PerfActionDpc;
    /* 0x0150 */ volatile long PerfActionMask;
    /* 0x0154 */ long Padding_190;
    /* 0x0158 */ struct _PROC_IDLE_SNAP HvIdleCheck;
    /* 0x0168 */ struct _PROC_PERF_CHECK_CONTEXT CheckContext;
    /* 0x01a8 */ struct _PPM_CONCURRENCY_ACCOUNTING* Concurrency;
    /* 0x01b0 */ struct _PPM_CONCURRENCY_ACCOUNTING* ClassConcurrency;
    /* 0x01b8 */ unsigned char ArchitecturalEfficiencyClass;
    /* 0x01b9 */ unsigned char PerformanceSchedulingClass;
    /* 0x01ba */ unsigned char EfficiencySchedulingClass;
    /* 0x01bb */ unsigned char Unused;
    /* 0x01bc */ unsigned char Parked;
    /* 0x01bd */ unsigned char LongPriorQosPeriod;
    /* 0x01be */ char Padding_191[2];
    union
    {
        /* 0x01c0 */ unsigned __int64 SnapTimeLast;
        /* 0x01c0 */ unsigned __int64 EnergyConsumed;
    }; /* size: 0x0008 */
    /* 0x01c8 */ unsigned __int64 ActiveTime;
    /* 0x01d0 */ unsigned __int64 TotalTime;
    /* 0x01d8 */ struct _POP_FX_DEVICE* FxDevice;
    /* 0x01e0 */ unsigned __int64 LastQosTranstionTsc;
    /* 0x01e8 */ unsigned __int64 QosTransitionHysteresis;
    /* 0x01f0 */ enum _KHETERO_CPU_QOS RequestedQosClass;
    /* 0x01f4 */ enum _KHETERO_CPU_QOS ResolvedQosClass;
    /* 0x01f8 */ unsigned short QosEquivalencyMask;
    /* 0x01fa */ unsigned short HwFeedbackTableIndex;
    /* 0x01fc */ unsigned char HwFeedbackParkHint;
    /* 0x01fd */ unsigned char HwFeedbackPerformanceClass;
    /* 0x01fe */ unsigned char HwFeedbackEfficiencyClass;
    /* 0x01ff */ unsigned char HeteroCoreType;
} PROCESSOR_POWER_STATE, * PPROCESSOR_POWER_STATE; /* size: 0x0200 */

typedef struct _MACHINE_FRAME
{
    /* 0x0000 */ unsigned __int64 Rip;
    /* 0x0008 */ unsigned short SegCs;
    /* 0x000a */ unsigned short Fill1[3];
    /* 0x0010 */ unsigned long EFlags;
    /* 0x0014 */ unsigned long Fill2;
    /* 0x0018 */ unsigned __int64 Rsp;
    /* 0x0020 */ unsigned short SegSs;
    /* 0x0022 */ unsigned short Fill3[3];
} MACHINE_FRAME, * PMACHINE_FRAME; /* size: 0x0028 */

typedef struct _MACHINE_CHECK_CONTEXT
{
    /* 0x0000 */ struct _MACHINE_FRAME MachineFrame;
    /* 0x0028 */ unsigned __int64 Rax;
    /* 0x0030 */ unsigned __int64 Rcx;
    /* 0x0038 */ unsigned __int64 Rdx;
    /* 0x0040 */ unsigned __int64 GsBase;
    /* 0x0048 */ unsigned __int64 Cr3;
} MACHINE_CHECK_CONTEXT, * PMACHINE_CHECK_CONTEXT; /* size: 0x0050 */

typedef struct _KTIMER_EXPIRATION_TRACE
{
    /* 0x0000 */ unsigned __int64 InterruptTime;
    /* 0x0008 */ union _LARGE_INTEGER PerformanceCounter;
} KTIMER_EXPIRATION_TRACE, * PKTIMER_EXPIRATION_TRACE; /* size: 0x0010 */

typedef struct _KSHARED_READY_QUEUE
{
    /* 0x0000 */ unsigned __int64 Lock;
    /* 0x0008 */ unsigned long ReadySummary;
    /* 0x000c */ long Padding_101;
    /* 0x0010 */ struct _LIST_ENTRY ReadyListHead[32];
    /* 0x0210 */ char RunningSummary[64];
    /* 0x0250 */ unsigned char Span;
    /* 0x0251 */ unsigned char LowProcIndex;
    /* 0x0252 */ unsigned char QueueIndex;
    /* 0x0253 */ unsigned char ProcCount;
    /* 0x0254 */ unsigned char ScanOwner;
    /* 0x0255 */ unsigned char Spare[3];
    /* 0x0258 */ unsigned __int64 Affinity;
    /* 0x0260 */ unsigned long ReadyThreadCount;
    /* 0x0264 */ long Padding_102;
    /* 0x0268 */ unsigned __int64 ReadyQueueExpectedRunTime;
} KSHARED_READY_QUEUE, * PKSHARED_READY_QUEUE; /* size: 0x0270 */

typedef struct _KSECURE_FAULT_INFORMATION
{
    /* 0x0000 */ unsigned __int64 FaultCode;
    /* 0x0008 */ unsigned __int64 FaultVa;
} KSECURE_FAULT_INFORMATION, * PKSECURE_FAULT_INFORMATION; /* size: 0x0010 */

typedef struct _IOP_IRP_STACK_PROFILER
{
    /* 0x0000 */ unsigned long Profile[20];
    /* 0x0050 */ unsigned long TotalIrps;
} IOP_IRP_STACK_PROFILER, * PIOP_IRP_STACK_PROFILER; /* size: 0x0054 */

typedef struct _KENTROPY_TIMING_STATE
{
    /* 0x0000 */ unsigned long EntropyCount;
    /* 0x0004 */ unsigned long Buffer[64];
    /* 0x0104 */ long Padding_87;
    /* 0x0108 */ struct _KDPC Dpc;
    /* 0x0148 */ unsigned long LastDeliveredBuffer;
    /* 0x014c */ long __PADDING__[1];
} KENTROPY_TIMING_STATE, * PKENTROPY_TIMING_STATE; /* size: 0x0150 */

typedef struct _FILESYSTEM_DISK_COUNTERS
{
    /* 0x0000 */ unsigned __int64 FsBytesRead;
    /* 0x0008 */ unsigned __int64 FsBytesWritten;
} FILESYSTEM_DISK_COUNTERS, * PFILESYSTEM_DISK_COUNTERS; /* size: 0x0010 */


typedef struct _SYNCH_COUNTERS
{
    /* 0x0000 */ unsigned long SpinLockAcquireCount;
    /* 0x0004 */ unsigned long SpinLockContentionCount;
    /* 0x0008 */ unsigned long SpinLockSpinCount;
    /* 0x000c */ unsigned long IpiSendRequestBroadcastCount;
    /* 0x0010 */ unsigned long IpiSendRequestRoutineCount;
    /* 0x0014 */ unsigned long IpiSendSoftwareInterruptCount;
    /* 0x0018 */ unsigned long ExInitializeResourceCount;
    /* 0x001c */ unsigned long ExReInitializeResourceCount;
    /* 0x0020 */ unsigned long ExDeleteResourceCount;
    /* 0x0024 */ unsigned long ExecutiveResourceAcquiresCount;
    /* 0x0028 */ unsigned long ExecutiveResourceContentionsCount;
    /* 0x002c */ unsigned long ExecutiveResourceReleaseExclusiveCount;
    /* 0x0030 */ unsigned long ExecutiveResourceReleaseSharedCount;
    /* 0x0034 */ unsigned long ExecutiveResourceConvertsCount;
    /* 0x0038 */ unsigned long ExAcqResExclusiveAttempts;
    /* 0x003c */ unsigned long ExAcqResExclusiveAcquiresExclusive;
    /* 0x0040 */ unsigned long ExAcqResExclusiveAcquiresExclusiveRecursive;
    /* 0x0044 */ unsigned long ExAcqResExclusiveWaits;
    /* 0x0048 */ unsigned long ExAcqResExclusiveNotAcquires;
    /* 0x004c */ unsigned long ExAcqResSharedAttempts;
    /* 0x0050 */ unsigned long ExAcqResSharedAcquiresExclusive;
    /* 0x0054 */ unsigned long ExAcqResSharedAcquiresShared;
    /* 0x0058 */ unsigned long ExAcqResSharedAcquiresSharedRecursive;
    /* 0x005c */ unsigned long ExAcqResSharedWaits;
    /* 0x0060 */ unsigned long ExAcqResSharedNotAcquires;
    /* 0x0064 */ unsigned long ExAcqResSharedStarveExclusiveAttempts;
    /* 0x0068 */ unsigned long ExAcqResSharedStarveExclusiveAcquiresExclusive;
    /* 0x006c */ unsigned long ExAcqResSharedStarveExclusiveAcquiresShared;
    /* 0x0070 */ unsigned long ExAcqResSharedStarveExclusiveAcquiresSharedRecursive;
    /* 0x0074 */ unsigned long ExAcqResSharedStarveExclusiveWaits;
    /* 0x0078 */ unsigned long ExAcqResSharedStarveExclusiveNotAcquires;
    /* 0x007c */ unsigned long ExAcqResSharedWaitForExclusiveAttempts;
    /* 0x0080 */ unsigned long ExAcqResSharedWaitForExclusiveAcquiresExclusive;
    /* 0x0084 */ unsigned long ExAcqResSharedWaitForExclusiveAcquiresShared;
    /* 0x0088 */ unsigned long ExAcqResSharedWaitForExclusiveAcquiresSharedRecursive;
    /* 0x008c */ unsigned long ExAcqResSharedWaitForExclusiveWaits;
    /* 0x0090 */ unsigned long ExAcqResSharedWaitForExclusiveNotAcquires;
    /* 0x0094 */ unsigned long ExSetResOwnerPointerExclusive;
    /* 0x0098 */ unsigned long ExSetResOwnerPointerSharedNew;
    /* 0x009c */ unsigned long ExSetResOwnerPointerSharedOld;
    /* 0x00a0 */ unsigned long ExTryToAcqExclusiveAttempts;
    /* 0x00a4 */ unsigned long ExTryToAcqExclusiveAcquires;
    /* 0x00a8 */ unsigned long ExBoostExclusiveOwner;
    /* 0x00ac */ unsigned long ExBoostSharedOwners;
    /* 0x00b0 */ unsigned long ExEtwSynchTrackingNotificationsCount;
    /* 0x00b4 */ unsigned long ExEtwSynchTrackingNotificationsAccountedCount;
} SYNCH_COUNTERS, * PSYNCH_COUNTERS; /* size: 0x00b8 */

typedef struct _KPRCB
{
    /* 0x0000 */ unsigned long MxCsr;
    /* 0x0004 */ unsigned char LegacyNumber;
    /* 0x0005 */ unsigned char ReservedMustBeZero;
    /* 0x0006 */ unsigned char InterruptRequest;
    /* 0x0007 */ unsigned char IdleHalt;
    /* 0x0008 */ struct _KTHREAD* CurrentThread;
    /* 0x0010 */ struct _KTHREAD* NextThread;
    /* 0x0018 */ struct _KTHREAD* IdleThread;
    /* 0x0020 */ unsigned char NestingLevel;
    /* 0x0021 */ unsigned char ClockOwner;
    union
    {
        /* 0x0022 */ unsigned char PendingTickFlags;
        struct /* bitfield */
        {
            /* 0x0022 */ unsigned char PendingTick : 1; /* bit position: 0 */
            /* 0x0022 */ unsigned char PendingBackupTick : 1; /* bit position: 1 */
        }; /* bitfield */
    }; /* size: 0x0001 */
    /* 0x0023 */ unsigned char IdleState;
    /* 0x0024 */ unsigned long Number;
    /* 0x0028 */ unsigned __int64 RspBase;
    /* 0x0030 */ unsigned __int64 PrcbLock;
    /* 0x0038 */ char* PriorityState;
    /* 0x0040 */ char CpuType;
    /* 0x0041 */ char CpuID;
    union
    {
        /* 0x0042 */ unsigned short CpuStep;
        struct
        {
            /* 0x0042 */ unsigned char CpuStepping;
            /* 0x0043 */ unsigned char CpuModel;
        }; /* size: 0x0002 */
    }; /* size: 0x0002 */
    /* 0x0044 */ unsigned long MHz;
    /* 0x0048 */ unsigned __int64 HalReserved[8];
    /* 0x0088 */ unsigned short MinorVersion;
    /* 0x008a */ unsigned short MajorVersion;
    /* 0x008c */ unsigned char BuildType;
    /* 0x008d */ unsigned char CpuVendor;
    /* 0x008e */ unsigned char LegacyCoresPerPhysicalProcessor;
    /* 0x008f */ unsigned char LegacyLogicalProcessorsPerCore;
    /* 0x0090 */ unsigned __int64 TscFrequency;
    /* 0x0098 */ unsigned long CoresPerPhysicalProcessor;
    /* 0x009c */ unsigned long LogicalProcessorsPerCore;
    /* 0x00a0 */ unsigned __int64 PrcbPad04[4];
    /* 0x00c0 */ struct _KNODE* ParentNode;
    /* 0x00c8 */ unsigned __int64 GroupSetMember;
    /* 0x00d0 */ unsigned char Group;
    /* 0x00d1 */ unsigned char GroupIndex;
    /* 0x00d2 */ unsigned char PrcbPad05[2];
    /* 0x00d4 */ unsigned long InitialApicId;
    /* 0x00d8 */ unsigned long ScbOffset;
    /* 0x00dc */ unsigned long ApicMask;
    /* 0x00e0 */ void* AcpiReserved;
    /* 0x00e8 */ unsigned long CFlushSize;
    /* 0x00ec */ union _KPRCBFLAG PrcbFlags;
    /* 0x00f0 */ unsigned __int64 PrcbPad11[2];
    /* 0x0100 */ struct _KPROCESSOR_STATE ProcessorState;
    /* 0x06c0 */ struct _XSAVE_AREA_HEADER* ExtendedSupervisorState;
    /* 0x06c8 */ unsigned long ProcessorSignature;
    /* 0x06cc */ unsigned long ProcessorFlags;
    union
    {
        struct
        {
            /* 0x06d0 */ unsigned short BpbRetpolineExitSpecCtrl;
            /* 0x06d2 */ unsigned short BpbTrappedRetpolineExitSpecCtrl;
            union
            {
                /* 0x06d4 */ unsigned short BpbTrappedBpbState;
                struct
                {
                    struct /* bitfield */
                    {
                        /* 0x06d4 */ unsigned short BpbTrappedCpuIdle : 1; /* bit position: 0 */
                        /* 0x06d4 */ unsigned short BpbTrappedFlushRsbOnTrap : 1; /* bit position: 1 */
                        /* 0x06d4 */ unsigned short BpbTrappedIbpbOnReturn : 1; /* bit position: 2 */
                        /* 0x06d4 */ unsigned short BpbTrappedIbpbOnTrap : 1; /* bit position: 3 */
                        /* 0x06d4 */ unsigned short BpbTrappedIbpbOnRetpolineExit : 1; /* bit position: 4 */
                        /* 0x06d4 */ unsigned short BpbTrappedFlushRsbOnReturn : 1; /* bit position: 5 */
                        /* 0x06d4 */ unsigned short BpbTrappedFlushRsbOnRetpolineExit : 1; /* bit position: 6 */
                        /* 0x06d4 */ unsigned short BpbTrappedBpbStateReserved : 1; /* bit position: 7 */
                        /* 0x06d4 */ unsigned short BpbTrappedBpbStateReserved2 : 8; /* bit position: 8 */
                    }; /* bitfield */
                    union
                    {
                        /* 0x06d6 */ unsigned char BpbRetpolineState;
                        struct
                        {
                            struct /* bitfield */
                            {
                                /* 0x06d6 */ unsigned char BpbRunningNonRetpolineCode : 1; /* bit position: 0 */
                                /* 0x06d6 */ unsigned char BpbIndirectCallsSafe : 1; /* bit position: 1 */
                                /* 0x06d6 */ unsigned char BpbRetpolineEnabled : 1; /* bit position: 2 */
                                /* 0x06d6 */ unsigned char BpbRetpolineStateReserved : 5; /* bit position: 3 */
                            }; /* bitfield */
                            /* 0x06d7 */ unsigned char PrcbPad12b;
                        }; /* size: 0x0002 */
                    }; /* size: 0x0002 */
                }; /* size: 0x0004 */
            }; /* size: 0x0004 */
        }; /* size: 0x0008 */
        /* 0x06d0 */ unsigned __int64 PrcbPad12a;
    }; /* size: 0x0008 */
    union
    {
        struct
        {
            /* 0x06d8 */ unsigned __int64 TrappedSecurityDomain;
            union
            {
                /* 0x06e0 */ unsigned short BpbState;
                struct
                {
                    struct /* bitfield */
                    {
                        /* 0x06e0 */ unsigned short BpbCpuIdle : 1; /* bit position: 0 */
                        /* 0x06e0 */ unsigned short BpbFlushRsbOnTrap : 1; /* bit position: 1 */
                        /* 0x06e0 */ unsigned short BpbIbpbOnReturn : 1; /* bit position: 2 */
                        /* 0x06e0 */ unsigned short BpbIbpbOnTrap : 1; /* bit position: 3 */
                        /* 0x06e0 */ unsigned short BpbIbpbOnRetpolineExit : 1; /* bit position: 4 */
                        /* 0x06e0 */ unsigned short BpbFlushRsbOnReturn : 1; /* bit position: 5 */
                        /* 0x06e0 */ unsigned short BpbFlushRsbOnRetpolineExit : 1; /* bit position: 6 */
                        /* 0x06e0 */ unsigned short BpbDivideOnReturn : 1; /* bit position: 7 */
                        /* 0x06e0 */ unsigned short VerwOnNonKvaReturn : 1; /* bit position: 8 */
                        /* 0x06e0 */ unsigned short FlushBhbOnTrap : 1; /* bit position: 9 */
                        /* 0x06e0 */ unsigned short Spare : 6; /* bit position: 10 */
                    }; /* bitfield */
                    union
                    {
                        /* 0x06e2 */ unsigned char BpbFeatures;
                        struct
                        {
                            struct /* bitfield */
                            {
                                /* 0x06e2 */ unsigned char BpbClearOnIdle : 1; /* bit position: 0 */
                                /* 0x06e2 */ unsigned char BpbEnabled : 1; /* bit position: 1 */
                                /* 0x06e2 */ unsigned char BpbSmep : 1; /* bit position: 2 */
                                /* 0x06e2 */ unsigned char BpbKCet : 1; /* bit position: 3 */
                                /* 0x06e2 */ unsigned char BhbFlushSequence : 2; /* bit position: 4 */
                                /* 0x06e2 */ unsigned char BpbFeaturesReserved : 2; /* bit position: 6 */
                            }; /* bitfield */
                            /* 0x06e3 */ unsigned char PrcbPad12e[1];
                            /* 0x06e4 */ unsigned short BpbCurrentSpecCtrl;
                            /* 0x06e6 */ unsigned short BpbKernelSpecCtrl;
                            /* 0x06e8 */ unsigned short BpbNmiSpecCtrl;
                            /* 0x06ea */ unsigned short BpbUserSpecCtrl;
                            /* 0x06ec */ volatile short PairRegister;
                            /* 0x06ee */ unsigned char PrcbPad12d[2];
                        }; /* size: 0x000e */
                    }; /* size: 0x000e */
                }; /* size: 0x0010 */
            }; /* size: 0x0010 */
        }; /* size: 0x0018 */
        /* 0x06d8 */ unsigned __int64 PrcbPad12c[3];
    }; /* size: 0x0018 */
    /* 0x06f0 */ struct _KSPIN_LOCK_QUEUE LockQueue[17];
    /* 0x0800 */ struct _PP_LOOKASIDE_LIST PPLookasideList[16];
    /* 0x0900 */ struct _GENERAL_LOOKASIDE_POOL PPNxPagedLookasideList[32];
    /* 0x1500 */ struct _GENERAL_LOOKASIDE_POOL PPNPagedLookasideList[32];
    /* 0x2100 */ struct _GENERAL_LOOKASIDE_POOL PPPagedLookasideList[32];
    /* 0x2d00 */ unsigned __int64 MsrIa32TsxCtrl;
    /* 0x2d08 */ struct _SINGLE_LIST_ENTRY DeferredReadyListHead;
    /* 0x2d10 */ volatile long MmPageFaultCount;
    /* 0x2d14 */ volatile long MmCopyOnWriteCount;
    /* 0x2d18 */ volatile long MmTransitionCount;
    /* 0x2d1c */ volatile long MmDemandZeroCount;
    /* 0x2d20 */ volatile long MmPageReadCount;
    /* 0x2d24 */ volatile long MmPageReadIoCount;
    /* 0x2d28 */ volatile long MmDirtyPagesWriteCount;
    /* 0x2d2c */ volatile long MmDirtyWriteIoCount;
    /* 0x2d30 */ volatile long MmMappedPagesWriteCount;
    /* 0x2d34 */ volatile long MmMappedWriteIoCount;
    /* 0x2d38 */ unsigned long KeSystemCalls;
    /* 0x2d3c */ unsigned long KeContextSwitches;
    /* 0x2d40 */ unsigned long PrcbPad40;
    /* 0x2d44 */ unsigned long CcFastReadNoWait;
    /* 0x2d48 */ unsigned long CcFastReadWait;
    /* 0x2d4c */ unsigned long CcFastReadNotPossible;
    /* 0x2d50 */ unsigned long CcCopyReadNoWait;
    /* 0x2d54 */ unsigned long CcCopyReadWait;
    /* 0x2d58 */ unsigned long CcCopyReadNoWaitMiss;
    /* 0x2d5c */ volatile long IoReadOperationCount;
    /* 0x2d60 */ volatile long IoWriteOperationCount;
    /* 0x2d64 */ volatile long IoOtherOperationCount;
    /* 0x2d68 */ union _LARGE_INTEGER IoReadTransferCount;
    /* 0x2d70 */ union _LARGE_INTEGER IoWriteTransferCount;
    /* 0x2d78 */ union _LARGE_INTEGER IoOtherTransferCount;
    /* 0x2d80 */ volatile long PacketBarrier;
    /* 0x2d84 */ volatile long TargetCount;
    /* 0x2d88 */ volatile unsigned long IpiFrozen;
    /* 0x2d8c */ unsigned long PrcbPad30;
    /* 0x2d90 */ void* IsrDpcStats;
    /* 0x2d98 */ unsigned long DeviceInterrupts;
    /* 0x2d9c */ long LookasideIrpFloat;
    /* 0x2da0 */ unsigned long InterruptLastCount;
    /* 0x2da4 */ unsigned long InterruptRate;
    /* 0x2da8 */ unsigned __int64 PrcbPad31;
    /* 0x2db0 */ struct _KPRCB* PairPrcb;
    /* 0x2db8 */ struct _KSTATIC_AFFINITY_BLOCK StaticAffinity;
    /* 0x3058 */ unsigned __int64 PrcbPad35[5];
    /* 0x3080 */ union _SLIST_HEADER InterruptObjectPool;
    /* 0x3090 */ struct _RTL_HASH_TABLE* DpcRuntimeHistoryHashTable;
    /* 0x3098 */ struct _KDPC* DpcRuntimeHistoryHashTableCleanupDpc;
    /* 0x30a0 */ void* CurrentDpcRoutine /* function */;
    /* 0x30a8 */ unsigned __int64 CurrentDpcRuntimeHistoryCached;
    /* 0x30b0 */ unsigned __int64 CurrentDpcStartTime;
    /* 0x30b8 */ unsigned __int64 PrcbPad41[1];
    /* 0x30c0 */ struct _KDPC_DATA DpcData[2];
    /* 0x3110 */ void* DpcStack;
    /* 0x3118 */ long MaximumDpcQueueDepth;
    /* 0x311c */ unsigned long DpcRequestRate;
    /* 0x3120 */ unsigned long MinimumDpcRate;
    /* 0x3124 */ unsigned long DpcLastCount;
    /* 0x3128 */ unsigned char ThreadDpcEnable;
    /* 0x3129 */ volatile unsigned char QuantumEnd;
    /* 0x312a */ volatile unsigned char DpcRoutineActive;
    /* 0x312b */ volatile unsigned char IdleSchedule;
    union
    {
        /* 0x312c */ volatile long DpcRequestSummary;
        /* 0x312c */ short DpcRequestSlot[2];
        struct
        {
            /* 0x312c */ short NormalDpcState;
            /* 0x312e */ short ThreadDpcState;
        }; /* size: 0x0004 */
        struct /* bitfield */
        {
            /* 0x312c */ unsigned long DpcNormalProcessingActive : 1; /* bit position: 0 */
            /* 0x312c */ unsigned long DpcNormalProcessingRequested : 1; /* bit position: 1 */
            /* 0x312c */ unsigned long DpcNormalThreadSignal : 1; /* bit position: 2 */
            /* 0x312c */ unsigned long DpcNormalTimerExpiration : 1; /* bit position: 3 */
            /* 0x312c */ unsigned long DpcNormalDpcPresent : 1; /* bit position: 4 */
            /* 0x312c */ unsigned long DpcNormalLocalInterrupt : 1; /* bit position: 5 */
            /* 0x312c */ unsigned long DpcNormalSpare : 10; /* bit position: 6 */
            /* 0x312c */ unsigned long DpcThreadActive : 1; /* bit position: 16 */
            /* 0x312c */ unsigned long DpcThreadRequested : 1; /* bit position: 17 */
            /* 0x312c */ unsigned long DpcThreadSpare : 14; /* bit position: 18 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x3130 */ unsigned long PrcbPad93;
    /* 0x3134 */ unsigned long LastTick;
    /* 0x3138 */ unsigned long ClockInterrupts;
    /* 0x313c */ unsigned long ReadyScanTick;
    /* 0x3140 */ void* InterruptObject[256];
    /* 0x3940 */ struct _KTIMER_TABLE TimerTable;
    /* 0x7b58 */ unsigned long PrcbPad92[10];
    /* 0x7b80 */ struct _KGATE DpcGate;
    /* 0x7b98 */ void* PrcbPad52;
    /* 0x7ba0 */ struct _KDPC CallDpc;
    /* 0x7be0 */ long ClockKeepAlive;
    /* 0x7be4 */ unsigned char PrcbPad60[2];
    union
    {
        struct
        {
            /* 0x7be6 */ unsigned char NmiActive;
            /* 0x7be7 */ unsigned char MceActive;
        }; /* size: 0x0002 */
        /* 0x7be6 */ unsigned short CombinedNmiMceActive;
    }; /* size: 0x0002 */
    /* 0x7be8 */ long DpcWatchdogPeriod;
    /* 0x7bec */ long DpcWatchdogCount;
    /* 0x7bf0 */ volatile long KeSpinLockOrdering;
    /* 0x7bf4 */ unsigned long DpcWatchdogProfileCumulativeDpcThreshold;
    /* 0x7bf8 */ void* CachedPtes;
    /* 0x7c00 */ struct _LIST_ENTRY WaitListHead;
    /* 0x7c10 */ unsigned __int64 WaitLock;
    /* 0x7c18 */ unsigned long ReadySummary;
    /* 0x7c1c */ long AffinitizedSelectionMask;
    /* 0x7c20 */ unsigned long QueueIndex;
    /* 0x7c24 */ unsigned long PrcbPad75[2];
    /* 0x7c2c */ unsigned long DpcWatchdogSequenceNumber;
    /* 0x7c30 */ struct _KDPC TimerExpirationDpc;
    /* 0x7c70 */ struct _RTL_RB_TREE ScbQueue;
    /* 0x7c80 */ struct _LIST_ENTRY DispatcherReadyListHead[32];
    /* 0x7e80 */ unsigned long InterruptCount;
    /* 0x7e84 */ unsigned long KernelTime;
    /* 0x7e88 */ unsigned long UserTime;
    /* 0x7e8c */ unsigned long DpcTime;
    /* 0x7e90 */ unsigned long InterruptTime;
    /* 0x7e94 */ unsigned long AdjustDpcThreshold;
    /* 0x7e98 */ unsigned char DebuggerSavedIRQL;
    /* 0x7e99 */ unsigned char GroupSchedulingOverQuota;
    /* 0x7e9a */ volatile unsigned char DeepSleep;
    /* 0x7e9b */ unsigned char PrcbPad80;
    /* 0x7e9c */ unsigned long DpcTimeCount;
    /* 0x7ea0 */ unsigned long DpcTimeLimit;
    /* 0x7ea4 */ unsigned long PeriodicCount;
    /* 0x7ea8 */ unsigned long PeriodicBias;
    /* 0x7eac */ unsigned long AvailableTime;
    /* 0x7eb0 */ unsigned long KeExceptionDispatchCount;
    /* 0x7eb4 */ unsigned long ReadyThreadCount;
    /* 0x7eb8 */ unsigned __int64 ReadyQueueExpectedRunTime;
    /* 0x7ec0 */ unsigned __int64 StartCycles;
    /* 0x7ec8 */ unsigned __int64 TaggedCyclesStart;
    /* 0x7ed0 */ unsigned __int64 TaggedCycles[3];
    /* 0x7ee8 */ unsigned __int64 AffinitizedCycles;
    /* 0x7ef0 */ unsigned __int64 ImportantCycles;
    /* 0x7ef8 */ unsigned __int64 UnimportantCycles;
    /* 0x7f00 */ unsigned long DpcWatchdogProfileSingleDpcThreshold;
    /* 0x7f04 */ volatile long MmSpinLockOrdering;
    /* 0x7f08 */ void* volatile CachedStack;
    /* 0x7f10 */ unsigned long PageColor;
    /* 0x7f14 */ unsigned long NodeColor;
    /* 0x7f18 */ unsigned long NodeShiftedColor;
    /* 0x7f1c */ unsigned long SecondaryColorMask;
    /* 0x7f20 */ unsigned char PrcbPad81[6];
    /* 0x7f26 */ unsigned char ExceptionStackActive;
    /* 0x7f27 */ unsigned char TbFlushListActive;
    /* 0x7f28 */ void* ExceptionStack;
    /* 0x7f30 */ unsigned __int64 PrcbPad82[1];
    /* 0x7f38 */ unsigned __int64 CycleTime;
    /* 0x7f40 */ unsigned __int64 Cycles[2][4];
    /* 0x7f80 */ unsigned long CcFastMdlReadNoWait;
    /* 0x7f84 */ unsigned long CcFastMdlReadWait;
    /* 0x7f88 */ unsigned long CcFastMdlReadNotPossible;
    /* 0x7f8c */ unsigned long CcMapDataNoWait;
    /* 0x7f90 */ unsigned long CcMapDataWait;
    /* 0x7f94 */ unsigned long CcPinMappedDataCount;
    /* 0x7f98 */ unsigned long CcPinReadNoWait;
    /* 0x7f9c */ unsigned long CcPinReadWait;
    /* 0x7fa0 */ unsigned long CcMdlReadNoWait;
    /* 0x7fa4 */ unsigned long CcMdlReadWait;
    /* 0x7fa8 */ unsigned long CcLazyWriteHotSpots;
    /* 0x7fac */ unsigned long CcLazyWriteIos;
    /* 0x7fb0 */ unsigned long CcLazyWritePages;
    /* 0x7fb4 */ unsigned long CcDataFlushes;
    /* 0x7fb8 */ unsigned long CcDataPages;
    /* 0x7fbc */ unsigned long CcLostDelayedWrites;
    /* 0x7fc0 */ unsigned long CcFastReadResourceMiss;
    /* 0x7fc4 */ unsigned long CcCopyReadWaitMiss;
    /* 0x7fc8 */ unsigned long CcFastMdlReadResourceMiss;
    /* 0x7fcc */ unsigned long CcMapDataNoWaitMiss;
    /* 0x7fd0 */ unsigned long CcMapDataWaitMiss;
    /* 0x7fd4 */ unsigned long CcPinReadNoWaitMiss;
    /* 0x7fd8 */ unsigned long CcPinReadWaitMiss;
    /* 0x7fdc */ unsigned long CcMdlReadNoWaitMiss;
    /* 0x7fe0 */ unsigned long CcMdlReadWaitMiss;
    /* 0x7fe4 */ unsigned long CcReadAheadIos;
    /* 0x7fe8 */ volatile long MmCacheTransitionCount;
    /* 0x7fec */ volatile long MmCacheReadCount;
    /* 0x7ff0 */ volatile long MmCacheIoCount;
    /* 0x7ff4 */ unsigned long PrcbPad91;
    /* 0x7ff8 */ void* MmInternal;
    /* 0x8000 */ struct _PROCESSOR_POWER_STATE PowerState;
    /* 0x8200 */ void* HyperPte;
    /* 0x8208 */ struct _LIST_ENTRY ScbList;
    /* 0x8218 */ struct _KDPC ForceIdleDpc;
    /* 0x8258 */ struct _KDPC DpcWatchdogDpc;
    /* 0x8298 */ struct _KTIMER DpcWatchdogTimer;
    /* 0x82d8 */ struct _CACHE_DESCRIPTOR Cache[5];
    /* 0x8314 */ unsigned long CacheCount;
    /* 0x8318 */ volatile unsigned long CachedCommit;
    /* 0x831c */ volatile unsigned long CachedResidentAvailable;
    /* 0x8320 */ void* WheaInfo;
    /* 0x8328 */ void* EtwSupport;
    /* 0x8330 */ void* ExSaPageArray;
    /* 0x8338 */ unsigned long KeAlignmentFixupCount;
    /* 0x833c */ unsigned long PrcbPad95;
    /* 0x8340 */ union _SLIST_HEADER HypercallPageList;
    /* 0x8350 */ unsigned __int64* StatisticsPage;
    /* 0x8358 */ unsigned __int64 GenerationTarget;
    /* 0x8360 */ unsigned __int64 PrcbPad85[4];
    /* 0x8380 */ void* HypercallCachedPages;
    /* 0x8388 */ void* VirtualApicAssist;
    /* 0x8390 */ struct _KAFFINITY_EX PackageProcessorSet;
    /* 0x8438 */ unsigned long PackageId;
    /* 0x843c */ unsigned long PrcbPad86;
    /* 0x8440 */ unsigned __int64 SharedReadyQueueMask;
    /* 0x8448 */ struct _KSHARED_READY_QUEUE* SharedReadyQueue;
    /* 0x8450 */ unsigned long SharedQueueScanOwner;
    /* 0x8454 */ unsigned long ScanSiblingIndex;
    /* 0x8458 */ unsigned __int64 CoreProcessorSet;
    /* 0x8460 */ unsigned __int64 ScanSiblingMask;
    /* 0x8468 */ unsigned __int64 LLCMask;
    /* 0x8470 */ unsigned __int64 CacheProcessorMask[5];
    /* 0x8498 */ struct _PROCESSOR_PROFILE_CONTROL_AREA* ProcessorProfileControlArea;
    /* 0x84a0 */ void* ProfileEventIndexAddress;
    /* 0x84a8 */ void** DpcWatchdogProfile;
    /* 0x84b0 */ void** DpcWatchdogProfileCurrentEmptyCapture;
    /* 0x84b8 */ void* SchedulerAssist;
    /* 0x84c0 */ struct _SYNCH_COUNTERS SynchCounters;
    /* 0x8578 */ unsigned __int64 PrcbPad94;
    /* 0x8580 */ struct _FILESYSTEM_DISK_COUNTERS FsCounters;
    /* 0x8590 */ unsigned char VendorString[13];
    /* 0x859d */ unsigned char PrcbPad100[3];
    /* 0x85a0 */ unsigned __int64 FeatureBits;
    /* 0x85a8 */ union _LARGE_INTEGER UpdateSignature;
    /* 0x85b0 */ unsigned __int64 PteBitCache;
    /* 0x85b8 */ unsigned long PteBitOffset;
    /* 0x85bc */ unsigned long PrcbPad105;
    /* 0x85c0 */ struct _CONTEXT* Context;
    /* 0x85c8 */ unsigned long ContextFlagsInit;
    /* 0x85cc */ unsigned long PrcbPad115;
    /* 0x85d0 */ struct _XSAVE_AREA* ExtendedState;
    /* 0x85d8 */ void* IsrStack;
    /* 0x85e0 */ struct _KENTROPY_TIMING_STATE EntropyTimingState;
    /* 0x8730 */ unsigned __int64 PrcbPad110;
    struct
    {
        /* 0x8738 */ unsigned long UpdateCycle;
        union
        {
            /* 0x873c */ short PairLocal;
            struct
            {
                /* 0x873c */ unsigned char PairLocalLow;
                struct /* bitfield */
                {
                    /* 0x873d */ unsigned char PairLocalForceStibp : 1; /* bit position: 0 */
                    /* 0x873d */ unsigned char Reserved : 4; /* bit position: 1 */
                    /* 0x873d */ unsigned char Frozen : 1; /* bit position: 5 */
                    /* 0x873d */ unsigned char ForceUntrusted : 1; /* bit position: 6 */
                    /* 0x873d */ unsigned char SynchIpi : 1; /* bit position: 7 */
                }; /* bitfield */
            }; /* size: 0x0002 */
        }; /* size: 0x0002 */
        union
        {
            /* 0x873e */ short PairRemote;
            struct
            {
                /* 0x873e */ unsigned char PairRemoteLow;
                /* 0x873f */ unsigned char Reserved2;
            }; /* size: 0x0002 */
        }; /* size: 0x0002 */
        /* 0x8740 */ unsigned char Trace[24];
        /* 0x8758 */ unsigned __int64 LocalDomain;
        /* 0x8760 */ unsigned __int64 RemoteDomain;
        /* 0x8768 */ struct _KTHREAD* Thread;
    } /* size: 0x0038 */ StibpPairingTrace;
    /* 0x8770 */ struct _SINGLE_LIST_ENTRY AbSelfIoBoostsList;
    /* 0x8778 */ struct _SINGLE_LIST_ENTRY AbPropagateBoostsList;
    /* 0x8780 */ struct _KDPC AbDpc;
    /* 0x87c0 */ struct _IOP_IRP_STACK_PROFILER IoIrpStackProfilerCurrent;
    /* 0x8814 */ struct _IOP_IRP_STACK_PROFILER IoIrpStackProfilerPrevious;
    /* 0x8868 */ struct _KSECURE_FAULT_INFORMATION SecureFault;
    /* 0x8878 */ unsigned __int64 PrcbPad120;
    /* 0x8880 */ struct _KSHARED_READY_QUEUE LocalSharedReadyQueue;
    /* 0x8af0 */ unsigned __int64 PrcbPad125[2];
    /* 0x8b00 */ unsigned long TimerExpirationTraceCount;
    /* 0x8b04 */ unsigned long PrcbPad127;
    /* 0x8b08 */ struct _KTIMER_EXPIRATION_TRACE TimerExpirationTrace[16];
    /* 0x8c08 */ unsigned __int64 PrcbPad128[7];
    /* 0x8c40 */ struct _REQUEST_MAILBOX* Mailbox;
    /* 0x8c48 */ unsigned __int64 PrcbPad130[7];
    /* 0x8c80 */ struct _MACHINE_CHECK_CONTEXT McheckContext[2];
    /* 0x8d20 */ unsigned __int64 PrcbPad134[4];
    /* 0x8d40 */ struct _KLOCK_QUEUE_HANDLE SelfmapLockHandle[4];
    /* 0x8da0 */ unsigned __int64 PrcbPad134a[4];
    /* 0x8dc0 */ unsigned char PrcbPad138[128];
    /* 0x8e40 */ unsigned char PrcbPad138a[64];
    /* 0x8e80 */ unsigned __int64 KernelDirectoryTableBase;
    /* 0x8e88 */ unsigned __int64 RspBaseShadow;
    /* 0x8e90 */ unsigned __int64 UserRspShadow;
    /* 0x8e98 */ unsigned long ShadowFlags;
    /* 0x8e9c */ unsigned long PrcbPad138b;
    /* 0x8ea0 */ unsigned __int64 PrcbPad138c;
    /* 0x8ea8 */ unsigned short PrcbPad138d;
    /* 0x8eaa */ unsigned short VerwSelector;
    /* 0x8eac */ unsigned long DbgMceNestingLevel;
    /* 0x8eb0 */ unsigned long DbgMceFlags;
    /* 0x8eb4 */ unsigned long PrcbPad139b;
    /* 0x8eb8 */ unsigned __int64 PrcbPad140[505];
    /* 0x9e80 */ unsigned __int64 PrcbPad140a[8];
    /* 0x9ec0 */ unsigned __int64 PrcbPad141[504];
    /* 0xae80 */ unsigned char PrcbPad141a[64];
    /* 0xaec0 */ struct _REQUEST_MAILBOX RequestMailbox[1];
} KPRCB, * PKPRCB; /* size: 0xaf00 */

typedef enum _OB_OPEN_REASON
{
    ObCreateHandle = 0,
    ObOpenHandle = 1,
    ObDuplicateHandle = 2,
    ObInheritHandle = 3,
    ObMaxOpenReason = 4,
} OB_OPEN_REASON, * POB_OPEN_REASON;


typedef struct _SECTION_IMAGE_INFORMATION
{
    /* 0x0000 */ void* TransferAddress;
    /* 0x0008 */ unsigned long ZeroBits;
    /* 0x000c */ long Padding_401;
    /* 0x0010 */ unsigned __int64 MaximumStackSize;
    /* 0x0018 */ unsigned __int64 CommittedStackSize;
    /* 0x0020 */ unsigned long SubSystemType;
    union
    {
        struct
        {
            /* 0x0024 */ unsigned short SubSystemMinorVersion;
            /* 0x0026 */ unsigned short SubSystemMajorVersion;
        }; /* size: 0x0004 */
        /* 0x0024 */ unsigned long SubSystemVersion;
    }; /* size: 0x0004 */
    union
    {
        struct
        {
            /* 0x0028 */ unsigned short MajorOperatingSystemVersion;
            /* 0x002a */ unsigned short MinorOperatingSystemVersion;
        }; /* size: 0x0004 */
        /* 0x0028 */ unsigned long OperatingSystemVersion;
    }; /* size: 0x0004 */
    /* 0x002c */ unsigned short ImageCharacteristics;
    /* 0x002e */ unsigned short DllCharacteristics;
    /* 0x0030 */ unsigned short Machine;
    /* 0x0032 */ unsigned char ImageContainsCode;
    union
    {
        /* 0x0033 */ unsigned char ImageFlags;
        struct /* bitfield */
        {
            /* 0x0033 */ unsigned char ComPlusNativeReady : 1; /* bit position: 0 */
            /* 0x0033 */ unsigned char ComPlusILOnly : 1; /* bit position: 1 */
            /* 0x0033 */ unsigned char ImageDynamicallyRelocated : 1; /* bit position: 2 */
            /* 0x0033 */ unsigned char ImageMappedFlat : 1; /* bit position: 3 */
            /* 0x0033 */ unsigned char BaseBelow4gb : 1; /* bit position: 4 */
            /* 0x0033 */ unsigned char ComPlusPrefer32bit : 1; /* bit position: 5 */
            /* 0x0033 */ unsigned char Reserved : 2; /* bit position: 6 */
        }; /* bitfield */
    }; /* size: 0x0001 */
    /* 0x0034 */ unsigned long LoaderFlags;
    /* 0x0038 */ unsigned long ImageFileSize;
    /* 0x003c */ unsigned long CheckSum;
} SECTION_IMAGE_INFORMATION, * PSECTION_IMAGE_INFORMATION; /* size: 0x0040 */


typedef struct _PORT_MESSAGE
{
    union
    {
        union
        {
            struct
            {
                /* 0x0000 */ short DataLength;
                /* 0x0002 */ short TotalLength;
            } /* size: 0x0004 */ s1;
            /* 0x0000 */ unsigned long Length;
        }; /* size: 0x0004 */
    } /* size: 0x0004 */ u1;
    union
    {
        union
        {
            struct
            {
                /* 0x0004 */ short Type;
                /* 0x0006 */ short DataInfoOffset;
            } /* size: 0x0004 */ s2;
            /* 0x0004 */ unsigned long ZeroInit;
        }; /* size: 0x0004 */
    } /* size: 0x0004 */ u2;
    union
    {
        /* 0x0008 */ struct _CLIENT_ID ClientId;
        /* 0x0008 */ double DoNotUseThisField;
    }; /* size: 0x0010 */
    /* 0x0018 */ unsigned long MessageId;
    /* 0x001c */ long Padding_201;
    union
    {
        /* 0x0020 */ unsigned __int64 ClientViewSize;
        struct
        {
            /* 0x0020 */ unsigned long CallbackId;
            /* 0x0024 */ long __PADDING__[1];
        }; /* size: 0x0008 */
    }; /* size: 0x0008 */
} PORT_MESSAGE, * PPORT_MESSAGE; /* size: 0x0028 */

typedef struct _AUX_ACCESS_DATA
{
    /* 0x0000 */ struct _PRIVILEGE_SET* PrivilegesUsed;
    /* 0x0008 */ struct _GENERIC_MAPPING GenericMapping;
    /* 0x0018 */ unsigned long AccessesToAudit;
    /* 0x001c */ unsigned long MaximumAuditMask;
    /* 0x0020 */ struct _GUID TransactionId;
    /* 0x0030 */ void* NewSecurityDescriptor;
    /* 0x0038 */ void* ExistingSecurityDescriptor;
    /* 0x0040 */ void* ParentSecurityDescriptor;
    /* 0x0048 */ void* DeRefSecurityDescriptor /* function */;
    /* 0x0050 */ void* SDLock;
    /* 0x0058 */ struct _ACCESS_REASONS AccessReasons;
    /* 0x00d8 */ unsigned char GenerateStagingEvents;
    /* 0x00d9 */ char __PADDING__[7];
} AUX_ACCESS_DATA, * PAUX_ACCESS_DATA; /* size: 0x00e0 */

typedef struct _PSP_OBJECT_CREATION_STATE
{
    /* 0x0000 */ struct _ACCESS_STATE AccessState;
    /* 0x00a0 */ struct _AUX_ACCESS_DATA AuxData;
    /* 0x0178 */ ULONG HandleAttributes;
    /* 0x017c */ KPROCESSOR_MODE PreviousMode;
    /* 0x0180 */ HANDLE NewHandle;
} PSP_OBJECT_CREATION_STATE, * PPSP_OBJECT_CREATION_STATE; /* size: 0x0188 */

typedef struct _EX_FAST_REF
{
    union
    {
        /* 0x0000 */ void* Object;
        /* 0x0000 */ unsigned __int64 RefCnt : 4; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 Value;
    }; /* size: 0x0008 */
} EX_FAST_REF, * PEX_FAST_REF; /* size: 0x0008 */

typedef struct _EX_CALLBACK
{
    /* 0x0000 */ struct _EX_FAST_REF RoutineBlock;
} EX_CALLBACK, * PEX_CALLBACK; /* size: 0x0008 */

typedef struct _EXHANDLE
{
    union
    {
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned long TagBits : 2; /* bit position: 0 */
            /* 0x0000 */ unsigned long Index : 30; /* bit position: 2 */
        }; /* bitfield */
        /* 0x0000 */ void* GenericHandleOverlay;
        /* 0x0000 */ unsigned __int64 Value;
    }; /* size: 0x0008 */
} EXHANDLE, * PEXHANDLE; /* size: 0x0008 */


typedef union _HANDLE_TABLE_ENTRY
{
    union
    {
        /* 0x0000 */ volatile __int64 VolatileLowValue;
        /* 0x0000 */ __int64 LowValue;
        struct
        {
            /* 0x0000 */ struct _HANDLE_TABLE_ENTRY_INFO* volatile InfoTable;
            union
            {
                /* 0x0008 */ __int64 HighValue;
                /* 0x0008 */ union _HANDLE_TABLE_ENTRY* NextFreeHandleEntry;
                /* 0x0008 */ struct _EXHANDLE LeafHandleValue;
            }; /* size: 0x0008 */
        }; /* size: 0x0010 */
        /* 0x0000 */ __int64 RefCountField;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0000 */ unsigned __int64 Unlocked : 1; /* bit position: 0 */
                /* 0x0000 */ unsigned __int64 RefCnt : 16; /* bit position: 1 */
                /* 0x0000 */ unsigned __int64 Attributes : 3; /* bit position: 17 */
                /* 0x0000 */ unsigned __int64 ObjectPointerBits : 44; /* bit position: 20 */
            }; /* bitfield */
            struct /* bitfield */
            {
                /* 0x0008 */ unsigned long GrantedAccessBits : 25; /* bit position: 0 */
                /* 0x0008 */ unsigned long NoRightsUpgrade : 1; /* bit position: 25 */
                /* 0x0008 */ unsigned long Spare1 : 6; /* bit position: 26 */
            }; /* bitfield */
            /* 0x000c */ unsigned long Spare2;
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
} HANDLE_TABLE_ENTRY, * PHANDLE_TABLE_ENTRY; /* size: 0x0010 */

typedef struct _ALPC_MESSAGE_ATTRIBUTES
{
    /* 0x0000 */ unsigned long AllocatedAttributes;
    /* 0x0004 */ unsigned long ValidAttributes;
} ALPC_MESSAGE_ATTRIBUTES, * PALPC_MESSAGE_ATTRIBUTES; /* size: 0x0008 */


typedef struct _MMSECTION_FLAGS
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned int BeingDeleted : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned int BeingCreated : 1; /* bit position: 1 */
        /* 0x0000 */ unsigned int BeingPurged : 1; /* bit position: 2 */
        /* 0x0000 */ unsigned int NoModifiedWriting : 1; /* bit position: 3 */
        /* 0x0000 */ unsigned int FailAllIo : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned int Image : 1; /* bit position: 5 */
        /* 0x0000 */ unsigned int Based : 1; /* bit position: 6 */
        /* 0x0000 */ unsigned int File : 1; /* bit position: 7 */
        /* 0x0000 */ unsigned int AttemptingDelete : 1; /* bit position: 8 */
        /* 0x0000 */ unsigned int PrefetchCreated : 1; /* bit position: 9 */
        /* 0x0000 */ unsigned int PhysicalMemory : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned int ImageControlAreaOnRemovableMedia : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned int Reserve : 1; /* bit position: 12 */
        /* 0x0000 */ unsigned int Commit : 1; /* bit position: 13 */
        /* 0x0000 */ unsigned int NoChange : 1; /* bit position: 14 */
        /* 0x0000 */ unsigned int WasPurged : 1; /* bit position: 15 */
        /* 0x0000 */ unsigned int UserReference : 1; /* bit position: 16 */
        /* 0x0000 */ unsigned int GlobalMemory : 1; /* bit position: 17 */
        /* 0x0000 */ unsigned int DeleteOnClose : 1; /* bit position: 18 */
        /* 0x0000 */ unsigned int FilePointerNull : 1; /* bit position: 19 */
        /* 0x0000 */ unsigned long PreferredNode : 6; /* bit position: 20 */
        /* 0x0000 */ unsigned int GlobalOnlyPerSession : 1; /* bit position: 26 */
        /* 0x0000 */ unsigned int UserWritable : 1; /* bit position: 27 */
        /* 0x0000 */ unsigned int SystemVaAllocated : 1; /* bit position: 28 */
        /* 0x0000 */ unsigned int PreferredFsCompressionBoundary : 1; /* bit position: 29 */
        /* 0x0000 */ unsigned int UsingFileExtents : 1; /* bit position: 30 */
        /* 0x0000 */ unsigned int PageSize64K : 1; /* bit position: 31 */
    }; /* bitfield */
} MMSECTION_FLAGS, * PMMSECTION_FLAGS; /* size: 0x0004 */

typedef struct _SECTION
{
    /* 0x0000 */ struct _RTL_BALANCED_NODE SectionNode;
    /* 0x0018 */ unsigned __int64 StartingVpn;
    /* 0x0020 */ unsigned __int64 EndingVpn;
    union
    {
        union
        {
            /* 0x0028 */ struct _CONTROL_AREA* ControlArea;
            /* 0x0028 */ struct _FILE_OBJECT* FileObject;
            struct /* bitfield */
            {
                /* 0x0028 */ unsigned __int64 RemoteImageFileObject : 1; /* bit position: 0 */
                /* 0x0028 */ unsigned __int64 RemoteDataFileObject : 1; /* bit position: 1 */
            }; /* bitfield */
        }; /* size: 0x0008 */
    } /* size: 0x0008 */ u1;
    /* 0x0030 */ unsigned __int64 SizeOfSection;
    union
    {
        union
        {
            /* 0x0038 */ unsigned long LongFlags;
            /* 0x0038 */ struct _MMSECTION_FLAGS Flags;
        }; /* size: 0x0004 */
    } /* size: 0x0004 */ u;
    struct /* bitfield */
    {
        /* 0x003c */ unsigned long InitialPageProtection : 12; /* bit position: 0 */
        /* 0x003c */ unsigned long SessionId : 19; /* bit position: 12 */
        /* 0x003c */ unsigned long NoValidationNeeded : 1; /* bit position: 31 */
    }; /* bitfield */
} SECTION, * PSECTION; /* size: 0x0040 */

typedef struct _CONTEXT_CHUNK
{
    /* 0x0000 */ long Offset;
    /* 0x0004 */ unsigned long Length;
} CONTEXT_CHUNK, * PCONTEXT_CHUNK; /* size: 0x0008 */

typedef struct _CONTEXT_EX
{
    /* 0x0000 */ struct _CONTEXT_CHUNK All;
    /* 0x0008 */ struct _CONTEXT_CHUNK Legacy;
    /* 0x0010 */ struct _CONTEXT_CHUNK XState;
} CONTEXT_EX, * PCONTEXT_EX; /* size: 0x0018 */


typedef struct _OBJECT_TYPE_INITIALIZER
{
    /* 0x0000 */ unsigned short Length;
    union
    {
        /* 0x0002 */ unsigned short ObjectTypeFlags;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0002 */ unsigned char CaseInsensitive : 1; /* bit position: 0 */
                /* 0x0002 */ unsigned char UnnamedObjectsOnly : 1; /* bit position: 1 */
                /* 0x0002 */ unsigned char UseDefaultObject : 1; /* bit position: 2 */
                /* 0x0002 */ unsigned char SecurityRequired : 1; /* bit position: 3 */
                /* 0x0002 */ unsigned char MaintainHandleCount : 1; /* bit position: 4 */
                /* 0x0002 */ unsigned char MaintainTypeList : 1; /* bit position: 5 */
                /* 0x0002 */ unsigned char SupportsObjectCallbacks : 1; /* bit position: 6 */
                /* 0x0002 */ unsigned char CacheAligned : 1; /* bit position: 7 */
            }; /* bitfield */
            struct /* bitfield */
            {
                /* 0x0003 */ unsigned char UseExtendedParameters : 1; /* bit position: 0 */
                /* 0x0003 */ unsigned char Reserved : 7; /* bit position: 1 */
            }; /* bitfield */
        }; /* size: 0x0002 */
    }; /* size: 0x0002 */
    /* 0x0004 */ unsigned long ObjectTypeCode;
    /* 0x0008 */ unsigned long InvalidAttributes;
    /* 0x000c */ struct _GENERIC_MAPPING GenericMapping;
    /* 0x001c */ unsigned long ValidAccessMask;
    /* 0x0020 */ unsigned long RetainAccess;
    /* 0x0024 */ enum _POOL_TYPE PoolType;
    /* 0x0028 */ unsigned long DefaultPagedPoolCharge;
    /* 0x002c */ unsigned long DefaultNonPagedPoolCharge;
    /* 0x0030 */ void* DumpProcedure /* function */;
    /* 0x0038 */ void* OpenProcedure /* function */;
    /* 0x0040 */ void* CloseProcedure /* function */;
    /* 0x0048 */ void* DeleteProcedure /* function */;
    union
    {
        /* 0x0050 */ void* ParseProcedure /* function */;
        /* 0x0050 */ void* ParseProcedureEx /* function */;
    }; /* size: 0x0008 */
    /* 0x0058 */ void* SecurityProcedure /* function */;
    /* 0x0060 */ void* QueryNameProcedure /* function */;
    /* 0x0068 */ void* OkayToCloseProcedure /* function */;
    /* 0x0070 */ unsigned long WaitObjectFlagMask;
    /* 0x0074 */ unsigned short WaitObjectFlagOffset;
    /* 0x0076 */ unsigned short WaitObjectPointerOffset;
} OBJECT_TYPE_INITIALIZER, * POBJECT_TYPE_INITIALIZER; /* size: 0x0078 */

typedef struct _GDI_TEB_BATCH
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned long Offset : 31; /* bit position: 0 */
        /* 0x0000 */ unsigned long HasRenderingCommand : 1; /* bit position: 31 */
    }; /* bitfield */
    /* 0x0004 */ long Padding_482;
    /* 0x0008 */ unsigned __int64 HDC;
    /* 0x0010 */ unsigned long Buffer[310];
} GDI_TEB_BATCH, * PGDI_TEB_BATCH; /* size: 0x04e8 */

typedef struct _ACTIVATION_CONTEXT_STACK
{
    /* 0x0000 */ struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME* ActiveFrame;
    /* 0x0008 */ struct _LIST_ENTRY FrameListCache;
    /* 0x0018 */ unsigned long Flags;
    /* 0x001c */ unsigned long NextCookieSequenceNumber;
    /* 0x0020 */ unsigned long StackId;
    /* 0x0024 */ long __PADDING__[1];
} ACTIVATION_CONTEXT_STACK, * PACTIVATION_CONTEXT_STACK; /* size: 0x0028 */

typedef struct _TEB
{
    /* 0x0000 */ struct _NT_TIB NtTib;
    /* 0x0038 */ void* EnvironmentPointer;
    /* 0x0040 */ struct _CLIENT_ID ClientId;
    /* 0x0050 */ void* ActiveRpcHandle;
    /* 0x0058 */ void* ThreadLocalStoragePointer;
    /* 0x0060 */ struct _PEB* ProcessEnvironmentBlock;
    /* 0x0068 */ unsigned long LastErrorValue;
    /* 0x006c */ unsigned long CountOfOwnedCriticalSections;
    /* 0x0070 */ void* CsrClientThread;
    /* 0x0078 */ void* Win32ThreadInfo;
    /* 0x0080 */ unsigned long User32Reserved[26];
    /* 0x00e8 */ unsigned long UserReserved[5];
    /* 0x00fc */ long Padding_483;
    /* 0x0100 */ void* WOW32Reserved;
    /* 0x0108 */ unsigned long CurrentLocale;
    /* 0x010c */ unsigned long FpSoftwareStatusRegister;
    /* 0x0110 */ void* ReservedForDebuggerInstrumentation[16];
    /* 0x0190 */ void* SystemReserved1[30];
    /* 0x0280 */ char PlaceholderCompatibilityMode;
    /* 0x0281 */ unsigned char PlaceholderHydrationAlwaysExplicit;
    /* 0x0282 */ char PlaceholderReserved[10];
    /* 0x028c */ unsigned long ProxiedProcessId;
    /* 0x0290 */ struct _ACTIVATION_CONTEXT_STACK _ActivationStack;
    /* 0x02b8 */ unsigned char WorkingOnBehalfTicket[8];
    /* 0x02c0 */ long ExceptionCode;
    /* 0x02c4 */ unsigned char Padding0[4];
    /* 0x02c8 */ struct _ACTIVATION_CONTEXT_STACK* ActivationContextStackPointer;
    /* 0x02d0 */ unsigned __int64 InstrumentationCallbackSp;
    /* 0x02d8 */ unsigned __int64 InstrumentationCallbackPreviousPc;
    /* 0x02e0 */ unsigned __int64 InstrumentationCallbackPreviousSp;
    /* 0x02e8 */ unsigned long TxFsContext;
    /* 0x02ec */ unsigned char InstrumentationCallbackDisabled;
    /* 0x02ed */ unsigned char UnalignedLoadStoreExceptions;
    /* 0x02ee */ unsigned char Padding1[2];
    /* 0x02f0 */ struct _GDI_TEB_BATCH GdiTebBatch;
    /* 0x07d8 */ struct _CLIENT_ID RealClientId;
    /* 0x07e8 */ void* GdiCachedProcessHandle;
    /* 0x07f0 */ unsigned long GdiClientPID;
    /* 0x07f4 */ unsigned long GdiClientTID;
    /* 0x07f8 */ void* GdiThreadLocalInfo;
    /* 0x0800 */ unsigned __int64 Win32ClientInfo[62];
    /* 0x09f0 */ void* glDispatchTable[233];
    /* 0x1138 */ unsigned __int64 glReserved1[29];
    /* 0x1220 */ void* glReserved2;
    /* 0x1228 */ void* glSectionInfo;
    /* 0x1230 */ void* glSection;
    /* 0x1238 */ void* glTable;
    /* 0x1240 */ void* glCurrentRC;
    /* 0x1248 */ void* glContext;
    /* 0x1250 */ unsigned long LastStatusValue;
    /* 0x1254 */ unsigned char Padding2[4];
    /* 0x1258 */ struct _UNICODE_STRING StaticUnicodeString;
    /* 0x1268 */ wchar_t StaticUnicodeBuffer[261];
    /* 0x1472 */ unsigned char Padding3[6];
    /* 0x1478 */ void* DeallocationStack;
    /* 0x1480 */ void* TlsSlots[64];
    /* 0x1680 */ struct _LIST_ENTRY TlsLinks;
    /* 0x1690 */ void* Vdm;
    /* 0x1698 */ void* ReservedForNtRpc;
    /* 0x16a0 */ void* DbgSsReserved[2];
    /* 0x16b0 */ unsigned long HardErrorMode;
    /* 0x16b4 */ unsigned char Padding4[4];
    /* 0x16b8 */ void* Instrumentation[11];
    /* 0x1710 */ struct _GUID ActivityId;
    /* 0x1720 */ void* SubProcessTag;
    /* 0x1728 */ void* PerflibData;
    /* 0x1730 */ void* EtwTraceData;
    /* 0x1738 */ void* WinSockData;
    /* 0x1740 */ unsigned long GdiBatchCount;
    union
    {
        /* 0x1744 */ struct _PROCESSOR_NUMBER CurrentIdealProcessor;
        /* 0x1744 */ unsigned long IdealProcessorValue;
        struct
        {
            /* 0x1744 */ unsigned char ReservedPad0;
            /* 0x1745 */ unsigned char ReservedPad1;
            /* 0x1746 */ unsigned char ReservedPad2;
            /* 0x1747 */ unsigned char IdealProcessor;
        }; /* size: 0x0004 */
    }; /* size: 0x0004 */
    /* 0x1748 */ unsigned long GuaranteedStackBytes;
    /* 0x174c */ unsigned char Padding5[4];
    /* 0x1750 */ void* ReservedForPerf;
    /* 0x1758 */ void* ReservedForOle;
    /* 0x1760 */ unsigned long WaitingOnLoaderLock;
    /* 0x1764 */ unsigned char Padding6[4];
    /* 0x1768 */ void* SavedPriorityState;
    /* 0x1770 */ unsigned __int64 ReservedForCodeCoverage;
    /* 0x1778 */ void* ThreadPoolData;
    /* 0x1780 */ void** TlsExpansionSlots;
    /* 0x1788 */ void* DeallocationBStore;
    /* 0x1790 */ void* BStoreLimit;
    /* 0x1798 */ unsigned long MuiGeneration;
    /* 0x179c */ unsigned long IsImpersonating;
    /* 0x17a0 */ void* NlsCache;
    /* 0x17a8 */ void* pShimData;
    /* 0x17b0 */ unsigned long HeapData;
    /* 0x17b4 */ unsigned char Padding7[4];
    /* 0x17b8 */ void* CurrentTransactionHandle;
    /* 0x17c0 */ struct _TEB_ACTIVE_FRAME* ActiveFrame;
    /* 0x17c8 */ void* FlsData;
    /* 0x17d0 */ void* PreferredLanguages;
    /* 0x17d8 */ void* UserPrefLanguages;
    /* 0x17e0 */ void* MergedPrefLanguages;
    /* 0x17e8 */ unsigned long MuiImpersonation;
    union
    {
        /* 0x17ec */ volatile unsigned short CrossTebFlags;
        /* 0x17ec */ unsigned short SpareCrossTebBits : 16; /* bit position: 0 */
    }; /* size: 0x0002 */
    union
    {
        /* 0x17ee */ unsigned short SameTebFlags;
        struct /* bitfield */
        {
            /* 0x17ee */ unsigned short SafeThunkCall : 1; /* bit position: 0 */
            /* 0x17ee */ unsigned short InDebugPrint : 1; /* bit position: 1 */
            /* 0x17ee */ unsigned short HasFiberData : 1; /* bit position: 2 */
            /* 0x17ee */ unsigned short SkipThreadAttach : 1; /* bit position: 3 */
            /* 0x17ee */ unsigned short WerInShipAssertCode : 1; /* bit position: 4 */
            /* 0x17ee */ unsigned short RanProcessInit : 1; /* bit position: 5 */
            /* 0x17ee */ unsigned short ClonedThread : 1; /* bit position: 6 */
            /* 0x17ee */ unsigned short SuppressDebugMsg : 1; /* bit position: 7 */
            /* 0x17ee */ unsigned short DisableUserStackWalk : 1; /* bit position: 8 */
            /* 0x17ee */ unsigned short RtlExceptionAttached : 1; /* bit position: 9 */
            /* 0x17ee */ unsigned short InitialThread : 1; /* bit position: 10 */
            /* 0x17ee */ unsigned short SessionAware : 1; /* bit position: 11 */
            /* 0x17ee */ unsigned short LoadOwner : 1; /* bit position: 12 */
            /* 0x17ee */ unsigned short LoaderWorker : 1; /* bit position: 13 */
            /* 0x17ee */ unsigned short SkipLoaderInit : 1; /* bit position: 14 */
            /* 0x17ee */ unsigned short SpareSameTebBits : 1; /* bit position: 15 */
        }; /* bitfield */
    }; /* size: 0x0002 */
    /* 0x17f0 */ void* TxnScopeEnterCallback;
    /* 0x17f8 */ void* TxnScopeExitCallback;
    /* 0x1800 */ void* TxnScopeContext;
    /* 0x1808 */ unsigned long LockCount;
    /* 0x180c */ long WowTebOffset;
    /* 0x1810 */ void* ResourceRetValue;
    /* 0x1818 */ void* ReservedForWdf;
    /* 0x1820 */ unsigned __int64 ReservedForCrt;
    /* 0x1828 */ struct _GUID EffectiveContainerId;
} TEB, * PTEB; /* size: 0x1838 */

#undef EX_PUSH_LOCK
#undef PEX_PUSH_LOCK
typedef struct _EX_PUSH_LOCK
{
    union
    {
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 Locked : 1; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 Waiting : 1; /* bit position: 1 */
            /* 0x0000 */ unsigned __int64 Waking : 1; /* bit position: 2 */
            /* 0x0000 */ unsigned __int64 MultipleShared : 1; /* bit position: 3 */
            /* 0x0000 */ unsigned __int64 Shared : 60; /* bit position: 4 */
        }; /* bitfield */
        /* 0x0000 */ unsigned __int64 Value;
        /* 0x0000 */ void* Ptr;
    }; /* size: 0x0008 */
} EX_PUSH_LOCK, * PEX_PUSH_LOCK; /* size: 0x0008 */

typedef struct _MMSECTION_FLAGS2
{
    /* 0x0000 */ unsigned short PartitionId : 10; /* bit position: 0 */
    struct /* bitfield */
    {
        /* 0x0002 */ unsigned char NoCrossPartitionAccess : 1; /* bit position: 0 */
        /* 0x0002 */ unsigned char SubsectionCrossPartitionReferenceOverflow : 1; /* bit position: 1 */
    }; /* bitfield */
    /* 0x0003 */ char __PADDING__[1];
} MMSECTION_FLAGS2, * PMMSECTION_FLAGS2; /* size: 0x0004 */

typedef struct _CONTROL_AREA
{
    /* 0x0000 */ struct _SEGMENT* Segment;
    union
    {
        /* 0x0008 */ struct _LIST_ENTRY ListHead;
        /* 0x0008 */ void* AweContext;
    }; /* size: 0x0010 */
    /* 0x0018 */ unsigned __int64 NumberOfSectionReferences;
    /* 0x0020 */ unsigned __int64 NumberOfPfnReferences;
    /* 0x0028 */ unsigned __int64 NumberOfMappedViews;
    /* 0x0030 */ unsigned __int64 NumberOfUserReferences;
    union
    {
        union
        {
            /* 0x0038 */ unsigned long LongFlags;
            /* 0x0038 */ struct _MMSECTION_FLAGS Flags;
        }; /* size: 0x0004 */
    } /* size: 0x0004 */ u;
    union
    {
        union
        {
            /* 0x003c */ unsigned long LongFlags;
            /* 0x003c */ struct _MMSECTION_FLAGS2 Flags;
        }; /* size: 0x0004 */
    } /* size: 0x0004 */ u1;
    /* 0x0040 */ struct _EX_FAST_REF FilePointer;
    /* 0x0048 */ volatile long ControlAreaLock;
    /* 0x004c */ unsigned long ModifiedWriteCount;
    /* 0x0050 */ struct _MI_CONTROL_AREA_WAIT_BLOCK* WaitList;
    union
    {
        struct
        {
            union
            {
                /* 0x0058 */ unsigned long NumberOfSystemCacheViews;
                /* 0x0058 */ unsigned long ImageRelocationStartBit;
            }; /* size: 0x0004 */
            union
            {
                /* 0x005c */ volatile long WritableUserReferences;
                struct /* bitfield */
                {
                    /* 0x005c */ unsigned long ImageRelocationSizeIn64k : 16; /* bit position: 0 */
                    /* 0x005c */ unsigned long SystemImage : 1; /* bit position: 16 */
                    /* 0x005c */ unsigned long CantMove : 1; /* bit position: 17 */
                    /* 0x005c */ unsigned long StrongCode : 2; /* bit position: 18 */
                    /* 0x005c */ unsigned long BitMap : 2; /* bit position: 20 */
                    /* 0x005c */ unsigned long ImageActive : 1; /* bit position: 22 */
                    /* 0x005c */ unsigned long ImageBaseOkToReuse : 1; /* bit position: 23 */
                }; /* bitfield */
            }; /* size: 0x0004 */
            union
            {
                /* 0x0060 */ unsigned long FlushInProgressCount;
                /* 0x0060 */ unsigned long NumberOfSubsections;
                /* 0x0060 */ struct _MI_IMAGE_SECURITY_REFERENCE* SeImageStub;
            }; /* size: 0x0008 */
        } /* size: 0x0010 */ e2;
    } /* size: 0x0010 */ u2;
    /* 0x0068 */ struct _EX_PUSH_LOCK FileObjectLock;
    /* 0x0070 */ volatile unsigned __int64 LockedPages;
    union
    {
        union
        {
            struct /* bitfield */
            {
                /* 0x0078 */ unsigned __int64 IoAttributionContext : 61; /* bit position: 0 */
                /* 0x0078 */ unsigned __int64 Spare : 3; /* bit position: 61 */
            }; /* bitfield */
            /* 0x0078 */ unsigned __int64 ImageCrossPartitionCharge;
            /* 0x0078 */ unsigned __int64 CommittedPageCount : 36; /* bit position: 0 */
        }; /* size: 0x0008 */
    } /* size: 0x0008 */ u3;
} CONTROL_AREA, * PCONTROL_AREA; /* size: 0x0080 */

typedef struct _EX_CALLBACK_ROUTINE_BLOCK
{
    /* 0x0000 */ struct _EX_RUNDOWN_REF RundownProtect;
    /* 0x0008 */ void* Function /* function */;
    /* 0x0010 */ void* Context;
} EX_CALLBACK_ROUTINE_BLOCK, * PEX_CALLBACK_ROUTINE_BLOCK; /* size: 0x0018 */


typedef union _KWAIT_STATUS_REGISTER
{
    union
    {
        /* 0x0000 */ unsigned char Flags;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned char State : 3; /* bit position: 0 */
            /* 0x0000 */ unsigned char Affinity : 1; /* bit position: 3 */
            /* 0x0000 */ unsigned char Priority : 1; /* bit position: 4 */
            /* 0x0000 */ unsigned char Apc : 1; /* bit position: 5 */
            /* 0x0000 */ unsigned char UserApc : 1; /* bit position: 6 */
            /* 0x0000 */ unsigned char Alert : 1; /* bit position: 7 */
        }; /* bitfield */
    }; /* size: 0x0001 */
} KWAIT_STATUS_REGISTER, * PKWAIT_STATUS_REGISTER; /* size: 0x0001 */



typedef union _KLOCK_ENTRY_BOOST_BITMAP
{
    union
    {
        /* 0x0000 */ unsigned long AllFields;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned long AllBoosts : 17; /* bit position: 0 */
            /* 0x0000 */ unsigned long Reserved : 15; /* bit position: 17 */
        }; /* bitfield */
        struct
        {
            struct /* bitfield */
            {
                /* 0x0000 */ unsigned short CpuBoostsBitmap : 15; /* bit position: 0 */
                /* 0x0000 */ unsigned short IoBoost : 1; /* bit position: 15 */
            }; /* bitfield */
            struct /* bitfield */
            {
                /* 0x0002 */ unsigned short IoQoSBoost : 1; /* bit position: 0 */
                /* 0x0002 */ unsigned short IoNormalPriorityWaiterCount : 8; /* bit position: 1 */
                /* 0x0002 */ unsigned short IoQoSWaiterCount : 7; /* bit position: 9 */
            }; /* bitfield */
        }; /* size: 0x0004 */
    }; /* size: 0x0004 */
} KLOCK_ENTRY_BOOST_BITMAP, * PKLOCK_ENTRY_BOOST_BITMAP; /* size: 0x0004 */


typedef struct _KLOCK_ENTRY_LOCK_STATE
{
    union
    {
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 CrossThreadReleasable : 1; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 Busy : 1; /* bit position: 1 */
            /* 0x0000 */ unsigned __int64 Reserved : 61; /* bit position: 2 */
            /* 0x0000 */ unsigned __int64 InTree : 1; /* bit position: 63 */
        }; /* bitfield */
        /* 0x0000 */ void* LockState;
    }; /* size: 0x0008 */
    union
    {
        /* 0x0008 */ void* SessionState;
        struct
        {
            /* 0x0008 */ unsigned long SessionId;
            /* 0x000c */ unsigned long SessionPad;
        }; /* size: 0x0008 */
    }; /* size: 0x0008 */
} KLOCK_ENTRY_LOCK_STATE, * PKLOCK_ENTRY_LOCK_STATE; /* size: 0x0010 */

typedef struct _KLOCK_ENTRY
{
    union
    {
        /* 0x0000 */ struct _RTL_BALANCED_NODE TreeNode;
        /* 0x0000 */ struct _SINGLE_LIST_ENTRY FreeListEntry;
    }; /* size: 0x0018 */
    union
    {
        /* 0x0018 */ unsigned long EntryFlags;
        struct
        {
            /* 0x0018 */ unsigned char EntryOffset;
            union
            {
                /* 0x0019 */ unsigned char ThreadLocalFlags;
                struct
                {
                    struct /* bitfield */
                    {
                        /* 0x0019 */ unsigned char WaitingBit : 1; /* bit position: 0 */
                        /* 0x0019 */ unsigned char Spare0 : 7; /* bit position: 1 */
                    }; /* bitfield */
                    union
                    {
                        /* 0x001a */ unsigned char AcquiredByte;
                        struct
                        {
                            /* 0x001a */ unsigned char AcquiredBit : 1; /* bit position: 0 */
                            union
                            {
                                /* 0x001b */ unsigned char CrossThreadFlags;
                                struct /* bitfield */
                                {
                                    /* 0x001b */ unsigned char HeadNodeBit : 1; /* bit position: 0 */
                                    /* 0x001b */ unsigned char IoPriorityBit : 1; /* bit position: 1 */
                                    /* 0x001b */ unsigned char IoQoSWaiter : 1; /* bit position: 2 */
                                    /* 0x001b */ unsigned char Spare1 : 5; /* bit position: 3 */
                                }; /* bitfield */
                            }; /* size: 0x0001 */
                        }; /* size: 0x0002 */
                    }; /* size: 0x0002 */
                }; /* size: 0x0003 */
            }; /* size: 0x0003 */
        }; /* size: 0x0004 */
        struct /* bitfield */
        {
            /* 0x0018 */ unsigned long StaticState : 8; /* bit position: 0 */
            /* 0x0018 */ unsigned long AllFlags : 24; /* bit position: 8 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x001c */ unsigned long SpareFlags;
    union
    {
        /* 0x0020 */ struct _KLOCK_ENTRY_LOCK_STATE LockState;
        /* 0x0020 */ void* volatile LockUnsafe;
        struct
        {
            /* 0x0020 */ volatile unsigned char CrossThreadReleasableAndBusyByte;
            /* 0x0021 */ unsigned char Reserved[6];
            /* 0x0027 */ volatile unsigned char InTreeByte;
            union
            {
                /* 0x0028 */ void* SessionState;
                struct
                {
                    /* 0x0028 */ unsigned long SessionId;
                    /* 0x002c */ unsigned long SessionPad;
                }; /* size: 0x0008 */
            }; /* size: 0x0008 */
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
    union
    {
        struct
        {
            /* 0x0030 */ struct _RTL_RB_TREE OwnerTree;
            /* 0x0040 */ struct _RTL_RB_TREE WaiterTree;
        }; /* size: 0x0020 */
        /* 0x0030 */ char CpuPriorityKey;
    }; /* size: 0x0020 */
    /* 0x0050 */ unsigned __int64 EntryLock;
    /* 0x0058 */ union _KLOCK_ENTRY_BOOST_BITMAP BoostBitmap;
    /* 0x005c */ unsigned long SparePad;
} KLOCK_ENTRY, * PKLOCK_ENTRY; /* size: 0x0060 */

typedef struct _PS_PROPERTY_SET
{
    /* 0x0000 */ struct _LIST_ENTRY ListHead;
    /* 0x0010 */ unsigned __int64 Lock;
} PS_PROPERTY_SET, * PPS_PROPERTY_SET; /* size: 0x0018 */


typedef struct _UMS_CONTROL_BLOCK
{
    /* 0x0000 */ struct _RTL_UMS_CONTEXT* UmsContext;
    /* 0x0008 */ struct _SINGLE_LIST_ENTRY* CompletionListEntry;
    /* 0x0010 */ struct _KEVENT* CompletionListEvent;
    /* 0x0018 */ unsigned long ServiceSequenceNumber;
    /* 0x001c */ long Padding_183;
    union
    {
        struct
        {
            /* 0x0020 */ struct _KQUEUE UmsQueue;
            /* 0x0060 */ struct _LIST_ENTRY QueueEntry;
            /* 0x0070 */ struct _RTL_UMS_CONTEXT* YieldingUmsContext;
            /* 0x0078 */ void* YieldingParam;
            /* 0x0080 */ void* UmsTeb;
        }; /* size: 0x0068 */
        struct
        {
            /* 0x0020 */ struct _KQUEUE* UmsAssociatedQueue;
            /* 0x0028 */ struct _LIST_ENTRY* UmsQueueListEntry;
            /* 0x0030 */ struct _KEVENT UmsWaitEvent;
            /* 0x0048 */ void* StagingArea;
            union
            {
                struct /* bitfield */
                {
                    /* 0x0050 */ unsigned long UmsPrimaryDeliveredContext : 1; /* bit position: 0 */
                    /* 0x0050 */ unsigned long UmsAssociatedQueueUsed : 1; /* bit position: 1 */
                    /* 0x0050 */ unsigned long UmsThreadParked : 1; /* bit position: 2 */
                }; /* bitfield */
                /* 0x0050 */ unsigned long UmsFlags;
            }; /* size: 0x0004 */
            /* 0x0054 */ long __PADDING__[13];
        }; /* size: 0x0068 */
    }; /* size: 0x0068 */
} UMS_CONTROL_BLOCK, * PUMS_CONTROL_BLOCK; /* size: 0x0088 */

typedef struct _KTHREAD
{
    /* 0x0000 */ struct _DISPATCHER_HEADER Header;
    /* 0x0018 */ void* SListFaultAddress;
    /* 0x0020 */ unsigned __int64 QuantumTarget;
    /* 0x0028 */ void* InitialStack;
    /* 0x0030 */ void* volatile StackLimit;
    /* 0x0038 */ void* StackBase;
    /* 0x0040 */ unsigned __int64 ThreadLock;
    /* 0x0048 */ volatile unsigned __int64 CycleTime;
    /* 0x0050 */ unsigned long CurrentRunTime;
    /* 0x0054 */ unsigned long ExpectedRunTime;
    /* 0x0058 */ void* KernelStack;
    /* 0x0060 */ struct _XSAVE_FORMAT* StateSaveArea;
    /* 0x0068 */ struct _KSCHEDULING_GROUP* volatile SchedulingGroup;
    /* 0x0070 */ union _KWAIT_STATUS_REGISTER WaitRegister;
    /* 0x0071 */ volatile unsigned char Running;
    /* 0x0072 */ unsigned char Alerted[2];
    union
    {
        struct /* bitfield */
        {
            /* 0x0074 */ unsigned long AutoBoostActive : 1; /* bit position: 0 */
            /* 0x0074 */ unsigned long ReadyTransition : 1; /* bit position: 1 */
            /* 0x0074 */ unsigned long WaitNext : 1; /* bit position: 2 */
            /* 0x0074 */ unsigned long SystemAffinityActive : 1; /* bit position: 3 */
            /* 0x0074 */ unsigned long Alertable : 1; /* bit position: 4 */
            /* 0x0074 */ unsigned long UserStackWalkActive : 1; /* bit position: 5 */
            /* 0x0074 */ unsigned long ApcInterruptRequest : 1; /* bit position: 6 */
            /* 0x0074 */ unsigned long QuantumEndMigrate : 1; /* bit position: 7 */
            /* 0x0074 */ unsigned long UmsDirectedSwitchEnable : 1; /* bit position: 8 */
            /* 0x0074 */ unsigned long TimerActive : 1; /* bit position: 9 */
            /* 0x0074 */ unsigned long SystemThread : 1; /* bit position: 10 */
            /* 0x0074 */ unsigned long ProcessDetachActive : 1; /* bit position: 11 */
            /* 0x0074 */ unsigned long CalloutActive : 1; /* bit position: 12 */
            /* 0x0074 */ unsigned long ScbReadyQueue : 1; /* bit position: 13 */
            /* 0x0074 */ unsigned long ApcQueueable : 1; /* bit position: 14 */
            /* 0x0074 */ unsigned long ReservedStackInUse : 1; /* bit position: 15 */
            /* 0x0074 */ unsigned long UmsPerformingSyscall : 1; /* bit position: 16 */
            /* 0x0074 */ unsigned long TimerSuspended : 1; /* bit position: 17 */
            /* 0x0074 */ unsigned long SuspendedWaitMode : 1; /* bit position: 18 */
            /* 0x0074 */ unsigned long SuspendSchedulerApcWait : 1; /* bit position: 19 */
            /* 0x0074 */ unsigned long CetUserShadowStack : 1; /* bit position: 20 */
            /* 0x0074 */ unsigned long BypassProcessFreeze : 1; /* bit position: 21 */
            /* 0x0074 */ unsigned long Reserved : 10; /* bit position: 22 */
        }; /* bitfield */
        /* 0x0074 */ long MiscFlags;
    }; /* size: 0x0004 */
    union
    {
        struct /* bitfield */
        {
            /* 0x0078 */ unsigned long ThreadFlagsSpare : 2; /* bit position: 0 */
            /* 0x0078 */ unsigned long AutoAlignment : 1; /* bit position: 2 */
            /* 0x0078 */ unsigned long DisableBoost : 1; /* bit position: 3 */
            /* 0x0078 */ unsigned long AlertedByThreadId : 1; /* bit position: 4 */
            /* 0x0078 */ unsigned long QuantumDonation : 1; /* bit position: 5 */
            /* 0x0078 */ unsigned long EnableStackSwap : 1; /* bit position: 6 */
            /* 0x0078 */ unsigned long GuiThread : 1; /* bit position: 7 */
            /* 0x0078 */ unsigned long DisableQuantum : 1; /* bit position: 8 */
            /* 0x0078 */ unsigned long ChargeOnlySchedulingGroup : 1; /* bit position: 9 */
            /* 0x0078 */ unsigned long DeferPreemption : 1; /* bit position: 10 */
            /* 0x0078 */ unsigned long QueueDeferPreemption : 1; /* bit position: 11 */
            /* 0x0078 */ unsigned long ForceDeferSchedule : 1; /* bit position: 12 */
            /* 0x0078 */ unsigned long SharedReadyQueueAffinity : 1; /* bit position: 13 */
            /* 0x0078 */ unsigned long FreezeCount : 1; /* bit position: 14 */
            /* 0x0078 */ unsigned long TerminationApcRequest : 1; /* bit position: 15 */
            /* 0x0078 */ unsigned long AutoBoostEntriesExhausted : 1; /* bit position: 16 */
            /* 0x0078 */ unsigned long KernelStackResident : 1; /* bit position: 17 */
            /* 0x0078 */ unsigned long TerminateRequestReason : 2; /* bit position: 18 */
            /* 0x0078 */ unsigned long ProcessStackCountDecremented : 1; /* bit position: 20 */
            /* 0x0078 */ unsigned long RestrictedGuiThread : 1; /* bit position: 21 */
            /* 0x0078 */ unsigned long VpBackingThread : 1; /* bit position: 22 */
            /* 0x0078 */ unsigned long ThreadFlagsSpare2 : 1; /* bit position: 23 */
            /* 0x0078 */ unsigned long EtwStackTraceApcInserted : 8; /* bit position: 24 */
        }; /* bitfield */
        /* 0x0078 */ volatile long ThreadFlags;
    }; /* size: 0x0004 */
    /* 0x007c */ volatile unsigned char Tag;
    /* 0x007d */ unsigned char SystemHeteroCpuPolicy;
    struct /* bitfield */
    {
        /* 0x007e */ unsigned char UserHeteroCpuPolicy : 7; /* bit position: 0 */
        /* 0x007e */ unsigned char ExplicitSystemHeteroCpuPolicy : 1; /* bit position: 7 */
    }; /* bitfield */
    union
    {
        struct /* bitfield */
        {
            /* 0x007f */ unsigned char RunningNonRetpolineCode : 1; /* bit position: 0 */
            /* 0x007f */ unsigned char SpecCtrlSpare : 7; /* bit position: 1 */
        }; /* bitfield */
        /* 0x007f */ unsigned char SpecCtrl;
    }; /* size: 0x0001 */
    /* 0x0080 */ unsigned long SystemCallNumber;
    /* 0x0084 */ unsigned long ReadyTime;
    /* 0x0088 */ void* FirstArgument;
    /* 0x0090 */ struct _KTRAP_FRAME* TrapFrame;
    union
    {
        /* 0x0098 */ struct _KAPC_STATE ApcState;
        struct
        {
            /* 0x0098 */ unsigned char ApcStateFill[43];
            /* 0x00c3 */ char Priority;
            /* 0x00c4 */ unsigned long UserIdealProcessor;
        }; /* size: 0x0030 */
    }; /* size: 0x0030 */
    /* 0x00c8 */ volatile __int64 WaitStatus;
    /* 0x00d0 */ struct _KWAIT_BLOCK* WaitBlockList;
    union
    {
        /* 0x00d8 */ struct _LIST_ENTRY WaitListEntry;
        /* 0x00d8 */ struct _SINGLE_LIST_ENTRY SwapListEntry;
    }; /* size: 0x0010 */
    /* 0x00e8 */ struct _DISPATCHER_HEADER* volatile Queue;
    /* 0x00f0 */ void* Teb;
    /* 0x00f8 */ unsigned __int64 RelativeTimerBias;
    /* 0x0100 */ struct _KTIMER Timer;
    union
    {
        /* 0x0140 */ struct _KWAIT_BLOCK WaitBlock[4];
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill4[20];
            /* 0x0154 */ unsigned long ContextSwitches;
        }; /* size: 0x0018 */
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill5[68];
            /* 0x0184 */ volatile unsigned char State;
            /* 0x0185 */ char Spare13;
            /* 0x0186 */ unsigned char WaitIrql;
            /* 0x0187 */ char WaitMode;
        }; /* size: 0x0048 */
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill6[116];
            /* 0x01b4 */ unsigned long WaitTime;
        }; /* size: 0x0078 */
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill7[164];
            union
            {
                struct
                {
                    /* 0x01e4 */ short KernelApcDisable;
                    /* 0x01e6 */ short SpecialApcDisable;
                }; /* size: 0x0004 */
                /* 0x01e4 */ unsigned long CombinedApcDisable;
            }; /* size: 0x0004 */
        }; /* size: 0x00a8 */
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill8[40];
            /* 0x0168 */ struct _KTHREAD_COUNTERS* ThreadCounters;
        }; /* size: 0x0030 */
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill9[88];
            /* 0x0198 */ struct _XSTATE_SAVE* XStateSave;
        }; /* size: 0x0060 */
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill10[136];
            /* 0x01c8 */ void* volatile Win32Thread;
        }; /* size: 0x0090 */
        struct
        {
            /* 0x0140 */ unsigned char WaitBlockFill11[176];
            /* 0x01f0 */ struct _UMS_CONTROL_BLOCK* Ucb;
            /* 0x01f8 */ struct _KUMS_CONTEXT_HEADER* volatile Uch;
        }; /* size: 0x00c0 */
    }; /* size: 0x00c0 */
    union
    {
        /* 0x0200 */ volatile long ThreadFlags2;
        struct /* bitfield */
        {
            /* 0x0200 */ unsigned long BamQosLevel : 8; /* bit position: 0 */
            /* 0x0200 */ unsigned long ThreadFlags2Reserved : 24; /* bit position: 8 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x0204 */ unsigned long Spare21;
    /* 0x0208 */ struct _LIST_ENTRY QueueListEntry;
    union
    {
        /* 0x0218 */ volatile unsigned long NextProcessor;
        struct /* bitfield */
        {
            /* 0x0218 */ unsigned long NextProcessorNumber : 31; /* bit position: 0 */
            /* 0x0218 */ unsigned long SharedReadyQueue : 1; /* bit position: 31 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x021c */ long QueuePriority;
    /* 0x0220 */ struct _KPROCESS* Process;
    union
    {
        /* 0x0228 */ struct _GROUP_AFFINITY UserAffinity;
        struct
        {
            /* 0x0228 */ unsigned char UserAffinityFill[10];
            /* 0x0232 */ char PreviousMode;
            /* 0x0233 */ char BasePriority;
            union
            {
                /* 0x0234 */ char PriorityDecrement;
                struct /* bitfield */
                {
                    /* 0x0234 */ unsigned char ForegroundBoost : 4; /* bit position: 0 */
                    /* 0x0234 */ unsigned char UnusualBoost : 4; /* bit position: 4 */
                }; /* bitfield */
            }; /* size: 0x0001 */
            /* 0x0235 */ unsigned char Preempted;
            /* 0x0236 */ unsigned char AdjustReason;
            /* 0x0237 */ char AdjustIncrement;
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
    /* 0x0238 */ unsigned __int64 AffinityVersion;
    union
    {
        /* 0x0240 */ struct _GROUP_AFFINITY Affinity;
        struct
        {
            /* 0x0240 */ unsigned char AffinityFill[10];
            /* 0x024a */ unsigned char ApcStateIndex;
            /* 0x024b */ unsigned char WaitBlockCount;
            /* 0x024c */ unsigned long IdealProcessor;
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
    /* 0x0250 */ unsigned __int64 NpxState;
    union
    {
        /* 0x0258 */ struct _KAPC_STATE SavedApcState;
        struct
        {
            /* 0x0258 */ unsigned char SavedApcStateFill[43];
            /* 0x0283 */ unsigned char WaitReason;
            /* 0x0284 */ char SuspendCount;
            /* 0x0285 */ char Saturation;
            /* 0x0286 */ unsigned short SListFaultCount;
        }; /* size: 0x0030 */
    }; /* size: 0x0030 */
    union
    {
        /* 0x0288 */ struct _KAPC SchedulerApc;
        struct
        {
            /* 0x0288 */ unsigned char SchedulerApcFill1[3];
            /* 0x028b */ unsigned char QuantumReset;
        }; /* size: 0x0004 */
        struct
        {
            /* 0x0288 */ unsigned char SchedulerApcFill2[4];
            /* 0x028c */ unsigned long KernelTime;
        }; /* size: 0x0008 */
        struct
        {
            /* 0x0288 */ unsigned char SchedulerApcFill3[64];
            /* 0x02c8 */ struct _KPRCB* volatile WaitPrcb;
        }; /* size: 0x0048 */
        struct
        {
            /* 0x0288 */ unsigned char SchedulerApcFill4[72];
            /* 0x02d0 */ void* LegoData;
        }; /* size: 0x0050 */
        struct
        {
            /* 0x0288 */ unsigned char SchedulerApcFill5[83];
            /* 0x02db */ unsigned char CallbackNestingLevel;
            /* 0x02dc */ unsigned long UserTime;
        }; /* size: 0x0058 */
    }; /* size: 0x0058 */
    /* 0x02e0 */ struct _KEVENT SuspendEvent;
    /* 0x02f8 */ struct _LIST_ENTRY ThreadListEntry;
    /* 0x0308 */ struct _LIST_ENTRY MutantListHead;
    /* 0x0318 */ unsigned char AbEntrySummary;
    /* 0x0319 */ unsigned char AbWaitEntryCount;
    /* 0x031a */ unsigned char AbAllocationRegionCount;
    /* 0x031b */ char SystemPriority;
    /* 0x031c */ unsigned long SecureThreadCookie;
    /* 0x0320 */ struct _KLOCK_ENTRY* LockEntries;
    /* 0x0328 */ struct _SINGLE_LIST_ENTRY PropagateBoostsEntry;
    /* 0x0330 */ struct _SINGLE_LIST_ENTRY IoSelfBoostsEntry;
    /* 0x0338 */ unsigned char PriorityFloorCounts[16];
    /* 0x0348 */ unsigned char PriorityFloorCountsReserved[16];
    /* 0x0358 */ unsigned long PriorityFloorSummary;
    /* 0x035c */ volatile long AbCompletedIoBoostCount;
    /* 0x0360 */ volatile long AbCompletedIoQoSBoostCount;
    /* 0x0364 */ volatile short KeReferenceCount;
    /* 0x0366 */ unsigned char AbOrphanedEntrySummary;
    /* 0x0367 */ unsigned char AbOwnedEntryCount;
    /* 0x0368 */ unsigned long ForegroundLossTime;
    /* 0x036c */ long Padding_172;
    union
    {
        /* 0x0370 */ struct _LIST_ENTRY GlobalForegroundListEntry;
        struct
        {
            /* 0x0370 */ struct _SINGLE_LIST_ENTRY ForegroundDpcStackListEntry;
            /* 0x0378 */ unsigned __int64 InGlobalForegroundList;
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
    /* 0x0380 */ __int64 ReadOperationCount;
    /* 0x0388 */ __int64 WriteOperationCount;
    /* 0x0390 */ __int64 OtherOperationCount;
    /* 0x0398 */ __int64 ReadTransferCount;
    /* 0x03a0 */ __int64 WriteTransferCount;
    /* 0x03a8 */ __int64 OtherTransferCount;
    /* 0x03b0 */ struct _KSCB* QueuedScb;
    /* 0x03b8 */ volatile unsigned long ThreadTimerDelay;
    union
    {
        /* 0x03bc */ volatile long ThreadFlags3;
        struct /* bitfield */
        {
            /* 0x03bc */ unsigned long ThreadFlags3Reserved : 8; /* bit position: 0 */
            /* 0x03bc */ unsigned long PpmPolicy : 2; /* bit position: 8 */
            /* 0x03bc */ unsigned long ThreadFlags3Reserved2 : 22; /* bit position: 10 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x03c0 */ unsigned __int64 TracingPrivate[1];
    /* 0x03c8 */ void* SchedulerAssist;
    /* 0x03d0 */ void* volatile AbWaitObject;
    /* 0x03d8 */ unsigned long ReservedPreviousReadyTimeValue;
    /* 0x03dc */ long Padding_173;
    /* 0x03e0 */ unsigned __int64 KernelWaitTime;
    /* 0x03e8 */ unsigned __int64 UserWaitTime;
    union
    {
        /* 0x03f0 */ struct _LIST_ENTRY GlobalUpdateVpThreadPriorityListEntry;
        struct
        {
            /* 0x03f0 */ struct _SINGLE_LIST_ENTRY UpdateVpThreadPriorityDpcStackListEntry;
            /* 0x03f8 */ unsigned __int64 InGlobalUpdateVpThreadPriorityList;
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
    /* 0x0400 */ long SchedulerAssistPriorityFloor;
    /* 0x0404 */ unsigned long Spare28;
    /* 0x0408 */ unsigned char ResourceIndex;
    /* 0x0409 */ unsigned char Spare31[3];
    /* 0x040c */ long Padding_174;
    /* 0x0410 */ unsigned __int64 EndPadding[4];
} KTHREAD, * PKTHREAD; /* size: 0x0430 */


typedef union _PS_CLIENT_SECURITY_CONTEXT
{
    union
    {
        /* 0x0000 */ unsigned __int64 ImpersonationData;
        /* 0x0000 */ void* ImpersonationToken;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 ImpersonationLevel : 2; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 EffectiveOnly : 1; /* bit position: 2 */
        }; /* bitfield */
    }; /* size: 0x0008 */
} PS_CLIENT_SECURITY_CONTEXT, * PPS_CLIENT_SECURITY_CONTEXT; /* size: 0x0008 */

typedef struct _ETHREAD
{
    /* 0x0000 */ struct _KTHREAD Tcb;
    /* 0x0430 */ union _LARGE_INTEGER CreateTime;
    union
    {
        /* 0x0438 */ union _LARGE_INTEGER ExitTime;
        /* 0x0438 */ struct _LIST_ENTRY KeyedWaitChain;
    }; /* size: 0x0010 */
    union
    {
        /* 0x0448 */ struct _LIST_ENTRY PostBlockList;
        struct
        {
            /* 0x0448 */ void* ForwardLinkShadow;
            /* 0x0450 */ void* StartAddress;
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
    union
    {
        /* 0x0458 */ struct _TERMINATION_PORT* TerminationPort;
        /* 0x0458 */ struct _ETHREAD* ReaperLink;
        /* 0x0458 */ void* KeyedWaitValue;
    }; /* size: 0x0008 */
    /* 0x0460 */ unsigned __int64 ActiveTimerListLock;
    /* 0x0468 */ struct _LIST_ENTRY ActiveTimerListHead;
    /* 0x0478 */ struct _CLIENT_ID Cid;
    union
    {
        /* 0x0488 */ struct _KSEMAPHORE KeyedWaitSemaphore;
        /* 0x0488 */ struct _KSEMAPHORE AlpcWaitSemaphore;
    }; /* size: 0x0020 */
    /* 0x04a8 */ union _PS_CLIENT_SECURITY_CONTEXT ClientSecurity;
    /* 0x04b0 */ struct _LIST_ENTRY IrpList;
    /* 0x04c0 */ unsigned __int64 TopLevelIrp;
    /* 0x04c8 */ struct _DEVICE_OBJECT* DeviceToVerify;
    /* 0x04d0 */ void* Win32StartAddress;
    /* 0x04d8 */ void* ChargeOnlySession;
    /* 0x04e0 */ void* LegacyPowerObject;
    /* 0x04e8 */ struct _LIST_ENTRY ThreadListEntry;
    /* 0x04f8 */ struct _EX_RUNDOWN_REF RundownProtect;
    /* 0x0500 */ struct _EX_PUSH_LOCK ThreadLock;
    /* 0x0508 */ unsigned long ReadClusterSize;
    /* 0x050c */ volatile long MmLockOrdering;
    union
    {
        /* 0x0510 */ unsigned long CrossThreadFlags;
        struct /* bitfield */
        {
            /* 0x0510 */ unsigned long Terminated : 1; /* bit position: 0 */
            /* 0x0510 */ unsigned long ThreadInserted : 1; /* bit position: 1 */
            /* 0x0510 */ unsigned long HideFromDebugger : 1; /* bit position: 2 */
            /* 0x0510 */ unsigned long ActiveImpersonationInfo : 1; /* bit position: 3 */
            /* 0x0510 */ unsigned long HardErrorsAreDisabled : 1; /* bit position: 4 */
            /* 0x0510 */ unsigned long BreakOnTermination : 1; /* bit position: 5 */
            /* 0x0510 */ unsigned long SkipCreationMsg : 1; /* bit position: 6 */
            /* 0x0510 */ unsigned long SkipTerminationMsg : 1; /* bit position: 7 */
            /* 0x0510 */ unsigned long CopyTokenOnOpen : 1; /* bit position: 8 */
            /* 0x0510 */ unsigned long ThreadIoPriority : 3; /* bit position: 9 */
            /* 0x0510 */ unsigned long ThreadPagePriority : 3; /* bit position: 12 */
            /* 0x0510 */ unsigned long RundownFail : 1; /* bit position: 15 */
            /* 0x0510 */ unsigned long UmsForceQueueTermination : 1; /* bit position: 16 */
            /* 0x0510 */ unsigned long IndirectCpuSets : 1; /* bit position: 17 */
            /* 0x0510 */ unsigned long DisableDynamicCodeOptOut : 1; /* bit position: 18 */
            /* 0x0510 */ unsigned long ExplicitCaseSensitivity : 1; /* bit position: 19 */
            /* 0x0510 */ unsigned long PicoNotifyExit : 1; /* bit position: 20 */
            /* 0x0510 */ unsigned long DbgWerUserReportActive : 1; /* bit position: 21 */
            /* 0x0510 */ unsigned long ForcedSelfTrimActive : 1; /* bit position: 22 */
            /* 0x0510 */ unsigned long SamplingCoverage : 1; /* bit position: 23 */
            /* 0x0510 */ unsigned long ReservedCrossThreadFlags : 8; /* bit position: 24 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    union
    {
        /* 0x0514 */ unsigned long SameThreadPassiveFlags;
        struct /* bitfield */
        {
            /* 0x0514 */ unsigned long ActiveExWorker : 1; /* bit position: 0 */
            /* 0x0514 */ unsigned long MemoryMaker : 1; /* bit position: 1 */
            /* 0x0514 */ unsigned long StoreLockThread : 2; /* bit position: 2 */
            /* 0x0514 */ unsigned long ClonedThread : 1; /* bit position: 4 */
            /* 0x0514 */ unsigned long KeyedEventInUse : 1; /* bit position: 5 */
            /* 0x0514 */ unsigned long SelfTerminate : 1; /* bit position: 6 */
            /* 0x0514 */ unsigned long RespectIoPriority : 1; /* bit position: 7 */
            /* 0x0514 */ unsigned long ActivePageLists : 1; /* bit position: 8 */
            /* 0x0514 */ unsigned long SecureContext : 1; /* bit position: 9 */
            /* 0x0514 */ unsigned long ZeroPageThread : 1; /* bit position: 10 */
            /* 0x0514 */ unsigned long WorkloadClass : 1; /* bit position: 11 */
            /* 0x0514 */ unsigned long ReservedSameThreadPassiveFlags : 20; /* bit position: 12 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    union
    {
        /* 0x0518 */ unsigned long SameThreadApcFlags;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0518 */ unsigned char OwnsProcessAddressSpaceExclusive : 1; /* bit position: 0 */
                /* 0x0518 */ unsigned char OwnsProcessAddressSpaceShared : 1; /* bit position: 1 */
                /* 0x0518 */ unsigned char HardFaultBehavior : 1; /* bit position: 2 */
                /* 0x0518 */ volatile unsigned char StartAddressInvalid : 1; /* bit position: 3 */
                /* 0x0518 */ unsigned char EtwCalloutActive : 1; /* bit position: 4 */
                /* 0x0518 */ unsigned char SuppressSymbolLoad : 1; /* bit position: 5 */
                /* 0x0518 */ unsigned char Prefetching : 1; /* bit position: 6 */
                /* 0x0518 */ unsigned char OwnsVadExclusive : 1; /* bit position: 7 */
            }; /* bitfield */
            struct /* bitfield */
            {
                /* 0x0519 */ unsigned char SystemPagePriorityActive : 1; /* bit position: 0 */
                /* 0x0519 */ unsigned char SystemPagePriority : 3; /* bit position: 1 */
                /* 0x0519 */ unsigned char AllowUserWritesToExecutableMemory : 1; /* bit position: 4 */
                /* 0x0519 */ unsigned char AllowKernelWritesToExecutableMemory : 1; /* bit position: 5 */
                /* 0x0519 */ unsigned char OwnsVadShared : 1; /* bit position: 6 */
            }; /* bitfield */
        }; /* size: 0x0002 */
    }; /* size: 0x0004 */
    /* 0x051c */ unsigned char CacheManagerActive;
    /* 0x051d */ unsigned char DisablePageFaultClustering;
    /* 0x051e */ unsigned char ActiveFaultCount;
    /* 0x051f */ unsigned char LockOrderState;
    /* 0x0520 */ unsigned long PerformanceCountLowReserved;
    /* 0x0524 */ long PerformanceCountHighReserved;
    /* 0x0528 */ unsigned __int64 AlpcMessageId;
    union
    {
        /* 0x0530 */ void* AlpcMessage;
        /* 0x0530 */ unsigned long AlpcReceiveAttributeSet;
    }; /* size: 0x0008 */
    /* 0x0538 */ struct _LIST_ENTRY AlpcWaitListEntry;
    /* 0x0548 */ long ExitStatus;
    /* 0x054c */ unsigned long CacheManagerCount;
    /* 0x0550 */ unsigned long IoBoostCount;
    /* 0x0554 */ unsigned long IoQoSBoostCount;
    /* 0x0558 */ unsigned long IoQoSThrottleCount;
    /* 0x055c */ unsigned long KernelStackReference;
    /* 0x0560 */ struct _LIST_ENTRY BoostList;
    /* 0x0570 */ struct _LIST_ENTRY DeboostList;
    /* 0x0580 */ unsigned __int64 BoostListLock;
    /* 0x0588 */ unsigned __int64 IrpListLock;
    /* 0x0590 */ void* ReservedForSynchTracking;
    /* 0x0598 */ struct _SINGLE_LIST_ENTRY CmCallbackListHead;
    /* 0x05a0 */ const struct _GUID* ActivityId;
    /* 0x05a8 */ struct _SINGLE_LIST_ENTRY SeLearningModeListHead;
    /* 0x05b0 */ void* VerifierContext;
    /* 0x05b8 */ void* AdjustedClientToken;
    /* 0x05c0 */ void* WorkOnBehalfThread;
    /* 0x05c8 */ struct _PS_PROPERTY_SET PropertySet;
    /* 0x05e0 */ void* PicoContext;
    /* 0x05e8 */ unsigned __int64 UserFsBase;
    /* 0x05f0 */ unsigned __int64 UserGsBase;
    /* 0x05f8 */ struct _THREAD_ENERGY_VALUES* EnergyValues;
    union
    {
        /* 0x0600 */ unsigned __int64 SelectedCpuSets;
        /* 0x0600 */ unsigned __int64* SelectedCpuSetsIndirect;
    }; /* size: 0x0008 */
    /* 0x0608 */ struct _EJOB* Silo;
    /* 0x0610 */ struct _UNICODE_STRING* ThreadName;
    /* 0x0618 */ struct _CONTEXT* SetContextState;
    /* 0x0620 */ unsigned long LastExpectedRunTime;
    /* 0x0624 */ unsigned long HeapData;
    /* 0x0628 */ struct _LIST_ENTRY OwnerEntryListHead;
    /* 0x0638 */ unsigned __int64 DisownedOwnerEntryListLock;
    /* 0x0640 */ struct _LIST_ENTRY DisownedOwnerEntryListHead;
    /* 0x0650 */ struct _KLOCK_ENTRY LockEntries[6];
    /* 0x0890 */ void* CmDbgInfo;
} ETHREAD, *PMY_ETHREAD; /* size: 0x0898 */

typedef struct _RTL_AVL_TREE
{
    /* 0x0000 */ struct _RTL_BALANCED_NODE* Root;
} RTL_AVL_TREE, * PRTL_AVL_TREE; /* size: 0x0008 */

typedef struct _PS_DYNAMIC_ENFORCED_ADDRESS_RANGES
{
    /* 0x0000 */ struct _RTL_AVL_TREE Tree;
    /* 0x0008 */ struct _EX_PUSH_LOCK Lock;
} PS_DYNAMIC_ENFORCED_ADDRESS_RANGES, * PPS_DYNAMIC_ENFORCED_ADDRESS_RANGES; /* size: 0x0010 */

typedef struct _JOBOBJECT_WAKE_FILTER
{
    /* 0x0000 */ unsigned long HighEdgeFilter;
    /* 0x0004 */ unsigned long LowEdgeFilter;
} JOBOBJECT_WAKE_FILTER, * PJOBOBJECT_WAKE_FILTER; /* size: 0x0008 */

typedef struct _PS_PROCESS_WAKE_INFORMATION
{
    /* 0x0000 */ unsigned __int64 NotificationChannel;
    /* 0x0008 */ unsigned long WakeCounters[7];
    /* 0x0024 */ struct _JOBOBJECT_WAKE_FILTER WakeFilter;
    /* 0x002c */ unsigned long NoWakeCounter;
} PS_PROCESS_WAKE_INFORMATION, * PPS_PROCESS_WAKE_INFORMATION; /* size: 0x0030 */

typedef union _PS_INTERLOCKED_TIMER_DELAY_VALUES
{
    union
    {
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 DelayMs : 30; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 CoalescingWindowMs : 30; /* bit position: 30 */
            /* 0x0000 */ unsigned __int64 Reserved : 1; /* bit position: 60 */
            /* 0x0000 */ unsigned __int64 NewTimerWheel : 1; /* bit position: 61 */
            /* 0x0000 */ unsigned __int64 Retry : 1; /* bit position: 62 */
            /* 0x0000 */ unsigned __int64 Locked : 1; /* bit position: 63 */
        }; /* bitfield */
        /* 0x0000 */ unsigned __int64 All;
    }; /* size: 0x0008 */
} PS_INTERLOCKED_TIMER_DELAY_VALUES, * PPS_INTERLOCKED_TIMER_DELAY_VALUES; /* size: 0x0008 */

typedef struct _PS_PROTECTION
{
    union
    {
        /* 0x0000 */ unsigned char Level;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned char Type : 3; /* bit position: 0 */
            /* 0x0000 */ unsigned char Audit : 1; /* bit position: 3 */
            /* 0x0000 */ unsigned char Signer : 4; /* bit position: 4 */
        }; /* bitfield */
    }; /* size: 0x0001 */
} PS_PROTECTION, * PPS_PROTECTION; /* size: 0x0001 */

typedef struct _ALPC_PROCESS_CONTEXT
{
    /* 0x0000 */ struct _EX_PUSH_LOCK Lock;
    /* 0x0008 */ struct _LIST_ENTRY ViewListHead;
    /* 0x0018 */ volatile unsigned __int64 PagedPoolQuotaCache;
} ALPC_PROCESS_CONTEXT, * PALPC_PROCESS_CONTEXT; /* size: 0x0020 */

typedef struct _MMSUPPORT_SHARED
{
    /* 0x0000 */ volatile long WorkingSetLock;
    /* 0x0004 */ long GoodCitizenWaiting;
    /* 0x0008 */ unsigned __int64 ReleasedCommitDebt;
    /* 0x0010 */ unsigned __int64 ResetPagesRepurposedCount;
    /* 0x0018 */ void* WsSwapSupport;
    /* 0x0020 */ void* CommitReleaseContext;
    /* 0x0028 */ void* AccessLog;
    /* 0x0030 */ volatile unsigned __int64 ChargedWslePages;
    /* 0x0038 */ unsigned __int64 ActualWslePages;
    /* 0x0040 */ unsigned __int64 WorkingSetCoreLock;
    /* 0x0048 */ void* ShadowMapping;
    /* 0x0050 */ long __PADDING__[12];
} MMSUPPORT_SHARED, * PMMSUPPORT_SHARED; /* size: 0x0080 */


typedef struct _MMSUPPORT_FLAGS
{
    union
    {
        struct
        {
            struct /* bitfield */
            {
                /* 0x0000 */ unsigned char WorkingSetType : 3; /* bit position: 0 */
                /* 0x0000 */ unsigned char Reserved0 : 3; /* bit position: 3 */
                /* 0x0000 */ unsigned char MaximumWorkingSetHard : 1; /* bit position: 6 */
                /* 0x0000 */ unsigned char MinimumWorkingSetHard : 1; /* bit position: 7 */
            }; /* bitfield */
            struct /* bitfield */
            {
                /* 0x0001 */ unsigned char SessionMaster : 1; /* bit position: 0 */
                /* 0x0001 */ unsigned char TrimmerState : 2; /* bit position: 1 */
                /* 0x0001 */ unsigned char Reserved : 1; /* bit position: 3 */
                /* 0x0001 */ unsigned char PageStealers : 4; /* bit position: 4 */
            }; /* bitfield */
        }; /* size: 0x0002 */
        /* 0x0000 */ unsigned short u1;
    }; /* size: 0x0002 */
    /* 0x0002 */ unsigned char MemoryPriority;
    union
    {
        struct /* bitfield */
        {
            /* 0x0003 */ unsigned char WsleDeleted : 1; /* bit position: 0 */
            /* 0x0003 */ unsigned char SvmEnabled : 1; /* bit position: 1 */
            /* 0x0003 */ unsigned char ForceAge : 1; /* bit position: 2 */
            /* 0x0003 */ unsigned char ForceTrim : 1; /* bit position: 3 */
            /* 0x0003 */ unsigned char NewMaximum : 1; /* bit position: 4 */
            /* 0x0003 */ unsigned char CommitReleaseState : 2; /* bit position: 5 */
        }; /* bitfield */
        /* 0x0003 */ unsigned char u2;
    }; /* size: 0x0001 */
} MMSUPPORT_FLAGS, * PMMSUPPORT_FLAGS; /* size: 0x0004 */

typedef struct _MMSUPPORT_INSTANCE
{
    /* 0x0000 */ unsigned long NextPageColor;
    /* 0x0004 */ unsigned long PageFaultCount;
    /* 0x0008 */ unsigned __int64 TrimmedPageCount;
    /* 0x0010 */ struct _MMWSL_INSTANCE* VmWorkingSetList;
    /* 0x0018 */ struct _LIST_ENTRY WorkingSetExpansionLinks;
    /* 0x0028 */ unsigned __int64 AgeDistribution[8];
    /* 0x0068 */ struct _KGATE* ExitOutswapGate;
    /* 0x0070 */ unsigned __int64 MinimumWorkingSetSize;
    /* 0x0078 */ unsigned __int64 WorkingSetLeafSize;
    /* 0x0080 */ unsigned __int64 WorkingSetLeafPrivateSize;
    /* 0x0088 */ unsigned __int64 WorkingSetSize;
    /* 0x0090 */ unsigned __int64 WorkingSetPrivateSize;
    /* 0x0098 */ unsigned __int64 MaximumWorkingSetSize;
    /* 0x00a0 */ unsigned __int64 PeakWorkingSetSize;
    /* 0x00a8 */ unsigned long HardFaultCount;
    /* 0x00ac */ unsigned short LastTrimStamp;
    /* 0x00ae */ unsigned short PartitionId;
    /* 0x00b0 */ unsigned __int64 SelfmapLock;
    /* 0x00b8 */ struct _MMSUPPORT_FLAGS Flags;
    /* 0x00bc */ long __PADDING__[1];
} MMSUPPORT_INSTANCE, * PMMSUPPORT_INSTANCE; /* size: 0x00c0 */

typedef struct _MMSUPPORT_FULL
{
    /* 0x0000 */ struct _MMSUPPORT_INSTANCE Instance;
    /* 0x00c0 */ struct _MMSUPPORT_SHARED Shared;
} MMSUPPORT_FULL, * PMMSUPPORT_FULL; /* size: 0x0140 */

typedef struct _SE_AUDIT_PROCESS_CREATION_INFO
{
    /* 0x0000 */ struct _OBJECT_NAME_INFORMATION* ImageFileName;
} SE_AUDIT_PROCESS_CREATION_INFO, * PSE_AUDIT_PROCESS_CREATION_INFO; /* size: 0x0008 */

typedef union _KSTACK_COUNT
{
    union
    {
        /* 0x0000 */ long Value;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned long State : 3; /* bit position: 0 */
            /* 0x0000 */ unsigned long StackCount : 29; /* bit position: 3 */
        }; /* bitfield */
    }; /* size: 0x0004 */
} KSTACK_COUNT, * PKSTACK_COUNT; /* size: 0x0004 */

typedef union _KEXECUTE_OPTIONS
{
    union
    {
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned char ExecuteDisable : 1; /* bit position: 0 */
            /* 0x0000 */ unsigned char ExecuteEnable : 1; /* bit position: 1 */
            /* 0x0000 */ unsigned char DisableThunkEmulation : 1; /* bit position: 2 */
            /* 0x0000 */ unsigned char Permanent : 1; /* bit position: 3 */
            /* 0x0000 */ unsigned char ExecuteDispatchEnable : 1; /* bit position: 4 */
            /* 0x0000 */ unsigned char ImageDispatchEnable : 1; /* bit position: 5 */
            /* 0x0000 */ unsigned char DisableExceptionChainValidation : 1; /* bit position: 6 */
            /* 0x0000 */ unsigned char Spare : 1; /* bit position: 7 */
        }; /* bitfield */
        /* 0x0000 */ volatile unsigned char ExecuteOptions;
        /* 0x0000 */ unsigned char ExecuteOptionsNV;
    }; /* size: 0x0001 */
} KEXECUTE_OPTIONS, * PKEXECUTE_OPTIONS; /* size: 0x0001 */

typedef struct _KPROCESS
{
    /* 0x0000 */ struct _DISPATCHER_HEADER Header;
    /* 0x0018 */ struct _LIST_ENTRY ProfileListHead;
    /* 0x0028 */ unsigned __int64 DirectoryTableBase;
    /* 0x0030 */ struct _LIST_ENTRY ThreadListHead;
    /* 0x0040 */ unsigned long ProcessLock;
    /* 0x0044 */ unsigned long ProcessTimerDelay;
    /* 0x0048 */ unsigned __int64 DeepFreezeStartTime;
    /* 0x0050 */ struct _KAFFINITY_EX Affinity;
    /* 0x00f8 */ unsigned __int64 AffinityPadding[12];
    /* 0x0158 */ struct _LIST_ENTRY ReadyListHead;
    /* 0x0168 */ struct _SINGLE_LIST_ENTRY SwapListEntry;
    /* 0x0170 */ volatile struct _KAFFINITY_EX ActiveProcessors;
    /* 0x0218 */ unsigned __int64 ActiveProcessorsPadding[12];
    union
    {
        struct /* bitfield */
        {
            /* 0x0278 */ unsigned long AutoAlignment : 1; /* bit position: 0 */
            /* 0x0278 */ unsigned long DisableBoost : 1; /* bit position: 1 */
            /* 0x0278 */ unsigned long DisableQuantum : 1; /* bit position: 2 */
            /* 0x0278 */ unsigned long DeepFreeze : 1; /* bit position: 3 */
            /* 0x0278 */ unsigned long TimerVirtualization : 1; /* bit position: 4 */
            /* 0x0278 */ unsigned long CheckStackExtents : 1; /* bit position: 5 */
            /* 0x0278 */ unsigned long CacheIsolationEnabled : 1; /* bit position: 6 */
            /* 0x0278 */ unsigned long PpmPolicy : 3; /* bit position: 7 */
            /* 0x0278 */ unsigned long VaSpaceDeleted : 1; /* bit position: 10 */
            /* 0x0278 */ unsigned long ReservedFlags : 21; /* bit position: 11 */
        }; /* bitfield */
        /* 0x0278 */ volatile long ProcessFlags;
    }; /* size: 0x0004 */
    /* 0x027c */ unsigned long ActiveGroupsMask;
    /* 0x0280 */ char BasePriority;
    /* 0x0281 */ char QuantumReset;
    /* 0x0282 */ char Visited;
    /* 0x0283 */ union _KEXECUTE_OPTIONS Flags;
    /* 0x0284 */ unsigned short ThreadSeed[20];
    /* 0x02ac */ unsigned short ThreadSeedPadding[12];
    /* 0x02c4 */ unsigned short IdealProcessor[20];
    /* 0x02ec */ unsigned short IdealProcessorPadding[12];
    /* 0x0304 */ unsigned short IdealNode[20];
    /* 0x032c */ unsigned short IdealNodePadding[12];
    /* 0x0344 */ unsigned short IdealGlobalNode;
    /* 0x0346 */ unsigned short Spare1;
    /* 0x0348 */ volatile union _KSTACK_COUNT StackCount;
    /* 0x034c */ long Padding_120;
    /* 0x0350 */ struct _LIST_ENTRY ProcessListEntry;
    /* 0x0360 */ unsigned __int64 CycleTime;
    /* 0x0368 */ unsigned __int64 ContextSwitches;
    /* 0x0370 */ struct _KSCHEDULING_GROUP* SchedulingGroup;
    /* 0x0378 */ unsigned long FreezeCount;
    /* 0x037c */ unsigned long KernelTime;
    /* 0x0380 */ unsigned long UserTime;
    /* 0x0384 */ unsigned long ReadyTime;
    /* 0x0388 */ unsigned __int64 UserDirectoryTableBase;
    /* 0x0390 */ unsigned char AddressPolicy;
    /* 0x0391 */ unsigned char Spare2[71];
    /* 0x03d8 */ void* InstrumentationCallback;
    union
    {
        union
        {
            /* 0x03e0 */ unsigned __int64 SecureHandle;
            struct
            {
                struct /* bitfield */
                {
                    /* 0x03e0 */ unsigned __int64 SecureProcess : 1; /* bit position: 0 */
                    /* 0x03e0 */ unsigned __int64 Unused : 1; /* bit position: 1 */
                }; /* bitfield */
            } /* size: 0x0008 */ Flags;
        }; /* size: 0x0008 */
    } /* size: 0x0008 */ SecureState;
    /* 0x03e8 */ unsigned __int64 KernelWaitTime;
    /* 0x03f0 */ unsigned __int64 UserWaitTime;
    /* 0x03f8 */ unsigned __int64 EndPadding[8];
} KPROCESS, * PKPROCESS; /* size: 0x0438 */

//
//typedef struct _PEB
//{
//  /* 0x0000 */ unsigned char InheritedAddressSpace;
//  /* 0x0001 */ unsigned char ReadImageFileExecOptions;
//  /* 0x0002 */ unsigned char BeingDebugged;
//  union
//  {
//    /* 0x0003 */ unsigned char BitField;
//    struct /* bitfield */
//    {
//      /* 0x0003 */ unsigned char ImageUsesLargePages : 1; /* bit position: 0 */
//      /* 0x0003 */ unsigned char IsProtectedProcess : 1; /* bit position: 1 */
//      /* 0x0003 */ unsigned char IsImageDynamicallyRelocated : 1; /* bit position: 2 */
//      /* 0x0003 */ unsigned char SkipPatchingUser32Forwarders : 1; /* bit position: 3 */
//      /* 0x0003 */ unsigned char IsPackagedProcess : 1; /* bit position: 4 */
//      /* 0x0003 */ unsigned char IsAppContainer : 1; /* bit position: 5 */
//      /* 0x0003 */ unsigned char IsProtectedProcessLight : 1; /* bit position: 6 */
//      /* 0x0003 */ unsigned char IsLongPathAwareProcess : 1; /* bit position: 7 */
//    }; /* bitfield */
//  }; /* size: 0x0001 */
//  /* 0x0004 */ unsigned char Padding0[4];
//  /* 0x0008 */ void* Mutant;
//  /* 0x0010 */ void* ImageBaseAddress;
//  /* 0x0018 */ struct _PEB_LDR_DATA* Ldr;
//  /* 0x0020 */ struct _RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
//  /* 0x0028 */ void* SubSystemData;
//  /* 0x0030 */ void* ProcessHeap;
//  /* 0x0038 */ struct _RTL_CRITICAL_SECTION* FastPebLock;
//  /* 0x0040 */ union _SLIST_HEADER* volatile AtlThunkSListPtr;
//  /* 0x0048 */ void* IFEOKey;
//  union
//  {
//    /* 0x0050 */ unsigned long CrossProcessFlags;
//    struct /* bitfield */
//    {
//      /* 0x0050 */ unsigned long ProcessInJob : 1; /* bit position: 0 */
//      /* 0x0050 */ unsigned long ProcessInitializing : 1; /* bit position: 1 */
//      /* 0x0050 */ unsigned long ProcessUsingVEH : 1; /* bit position: 2 */
//      /* 0x0050 */ unsigned long ProcessUsingVCH : 1; /* bit position: 3 */
//      /* 0x0050 */ unsigned long ProcessUsingFTH : 1; /* bit position: 4 */
//      /* 0x0050 */ unsigned long ProcessPreviouslyThrottled : 1; /* bit position: 5 */
//      /* 0x0050 */ unsigned long ProcessCurrentlyThrottled : 1; /* bit position: 6 */
//      /* 0x0050 */ unsigned long ProcessImagesHotPatched : 1; /* bit position: 7 */
//      /* 0x0050 */ unsigned long ReservedBits0 : 24; /* bit position: 8 */
//    }; /* bitfield */
//  }; /* size: 0x0004 */
//  /* 0x0054 */ unsigned char Padding1[4];
//  union
//  {
//    /* 0x0058 */ void* KernelCallbackTable;
//    /* 0x0058 */ void* UserSharedInfoPtr;
//  }; /* size: 0x0008 */
//  /* 0x0060 */ unsigned long SystemReserved;
//  /* 0x0064 */ unsigned long AtlThunkSListPtr32;
//  /* 0x0068 */ void* ApiSetMap;
//  /* 0x0070 */ unsigned long TlsExpansionCounter;
//  /* 0x0074 */ unsigned char Padding2[4];
//  /* 0x0078 */ void* TlsBitmap;
//  /* 0x0080 */ unsigned long TlsBitmapBits[2];
//  /* 0x0088 */ void* ReadOnlySharedMemoryBase;
//  /* 0x0090 */ void* SharedData;
//  /* 0x0098 */ void** ReadOnlyStaticServerData;
//  /* 0x00a0 */ void* AnsiCodePageData;
//  /* 0x00a8 */ void* OemCodePageData;
//  /* 0x00b0 */ void* UnicodeCaseTableData;
//  /* 0x00b8 */ unsigned long NumberOfProcessors;
//  /* 0x00bc */ unsigned long NtGlobalFlag;
//  /* 0x00c0 */ union _LARGE_INTEGER CriticalSectionTimeout;
//  /* 0x00c8 */ unsigned __int64 HeapSegmentReserve;
//  /* 0x00d0 */ unsigned __int64 HeapSegmentCommit;
//  /* 0x00d8 */ unsigned __int64 HeapDeCommitTotalFreeThreshold;
//  /* 0x00e0 */ unsigned __int64 HeapDeCommitFreeBlockThreshold;
//  /* 0x00e8 */ unsigned long NumberOfHeaps;
//  /* 0x00ec */ unsigned long MaximumNumberOfHeaps;
//  /* 0x00f0 */ void** ProcessHeaps;
//  /* 0x00f8 */ void* GdiSharedHandleTable;
//  /* 0x0100 */ void* ProcessStarterHelper;
//  /* 0x0108 */ unsigned long GdiDCAttributeList;
//  /* 0x010c */ unsigned char Padding3[4];
//  /* 0x0110 */ struct _RTL_CRITICAL_SECTION* LoaderLock;
//  /* 0x0118 */ unsigned long OSMajorVersion;
//  /* 0x011c */ unsigned long OSMinorVersion;
//  /* 0x0120 */ unsigned short OSBuildNumber;
//  /* 0x0122 */ unsigned short OSCSDVersion;
//  /* 0x0124 */ unsigned long OSPlatformId;
//  /* 0x0128 */ unsigned long ImageSubsystem;
//  /* 0x012c */ unsigned long ImageSubsystemMajorVersion;
//  /* 0x0130 */ unsigned long ImageSubsystemMinorVersion;
//  /* 0x0134 */ unsigned char Padding4[4];
//  /* 0x0138 */ unsigned __int64 ActiveProcessAffinityMask;
//  /* 0x0140 */ unsigned long GdiHandleBuffer[60];
//  /* 0x0230 */ void* PostProcessInitRoutine /* function */;
//  /* 0x0238 */ void* TlsExpansionBitmap;
//  /* 0x0240 */ unsigned long TlsExpansionBitmapBits[32];
//  /* 0x02c0 */ unsigned long SessionId;
//  /* 0x02c4 */ unsigned char Padding5[4];
//  /* 0x02c8 */ union _ULARGE_INTEGER AppCompatFlags;
//  /* 0x02d0 */ union _ULARGE_INTEGER AppCompatFlagsUser;
//  /* 0x02d8 */ void* pShimData;
//  /* 0x02e0 */ void* AppCompatInfo;
//  /* 0x02e8 */ struct _UNICODE_STRING CSDVersion;
//  /* 0x02f8 */ const struct _ACTIVATION_CONTEXT_DATA* ActivationContextData;
//  /* 0x0300 */ struct _ASSEMBLY_STORAGE_MAP* ProcessAssemblyStorageMap;
//  /* 0x0308 */ const struct _ACTIVATION_CONTEXT_DATA* SystemDefaultActivationContextData;
//  /* 0x0310 */ struct _ASSEMBLY_STORAGE_MAP* SystemAssemblyStorageMap;
//  /* 0x0318 */ unsigned __int64 MinimumStackCommit;
//  /* 0x0320 */ void* SparePointers[4];
//  /* 0x0340 */ unsigned long SpareUlongs[5];
//  /* 0x0354 */ long Padding_383;
//  /* 0x0358 */ void* WerRegistrationData;
//  /* 0x0360 */ void* WerShipAssertPtr;
//  /* 0x0368 */ void* pUnused;
//  /* 0x0370 */ void* pImageHeaderHash;
//  union
//  {
//    /* 0x0378 */ unsigned long TracingFlags;
//    struct /* bitfield */
//    {
//      /* 0x0378 */ unsigned long HeapTracingEnabled : 1; /* bit position: 0 */
//      /* 0x0378 */ unsigned long CritSecTracingEnabled : 1; /* bit position: 1 */
//      /* 0x0378 */ unsigned long LibLoaderTracingEnabled : 1; /* bit position: 2 */
//      /* 0x0378 */ unsigned long SpareTracingBits : 29; /* bit position: 3 */
//    }; /* bitfield */
//  }; /* size: 0x0004 */
//  /* 0x037c */ unsigned char Padding6[4];
//  /* 0x0380 */ unsigned __int64 CsrServerReadOnlySharedMemoryBase;
//  /* 0x0388 */ unsigned __int64 TppWorkerpListLock;
//  /* 0x0390 */ struct _LIST_ENTRY TppWorkerpList;
//  /* 0x03a0 */ void* WaitOnAddressHashTable[128];
//  /* 0x07a0 */ void* TelemetryCoverageHeader;
//  /* 0x07a8 */ unsigned long CloudFileFlags;
//  /* 0x07ac */ unsigned long CloudFileDiagFlags;
//  /* 0x07b0 */ char PlaceholderCompatibilityMode;
//  /* 0x07b1 */ char PlaceholderCompatibilityModeReserved[7];
//  /* 0x07b8 */ struct _LEAP_SECOND_DATA* LeapSecondData;
//  union
//  {
//    /* 0x07c0 */ unsigned long LeapSecondFlags;
//    struct /* bitfield */
//    {
//      /* 0x07c0 */ unsigned long SixtySecondEnabled : 1; /* bit position: 0 */
//      /* 0x07c0 */ unsigned long Reserved : 31; /* bit position: 1 */
//    }; /* bitfield */
//  }; /* size: 0x0004 */
//  /* 0x07c4 */ unsigned long NtGlobalFlag2;
//} PEB, *PPEB; /* size: 0x07c8 */


typedef struct _HANDLE_TABLE_FREE_LIST
{
    /* 0x0000 */ struct _EX_PUSH_LOCK FreeListLock;
    /* 0x0008 */ union _HANDLE_TABLE_ENTRY* FirstFreeHandleEntry;
    /* 0x0010 */ union _HANDLE_TABLE_ENTRY* LastFreeHandleEntry;
    /* 0x0018 */ long HandleCount;
    /* 0x001c */ unsigned long HighWaterMark;
    /* 0x0020 */ long __PADDING__[8];
} HANDLE_TABLE_FREE_LIST, * PHANDLE_TABLE_FREE_LIST; /* size: 0x0040 */


typedef struct _HANDLE_TABLE
{
    /* 0x0000 */ unsigned long NextHandleNeedingPool;
    /* 0x0004 */ long ExtraInfoPages;
    /* 0x0008 */ volatile unsigned __int64 TableCode;
    /* 0x0010 */ struct _EPROCESS* QuotaProcess;
    /* 0x0018 */ struct _LIST_ENTRY HandleTableList;
    /* 0x0028 */ unsigned long UniqueProcessId;
    union
    {
        /* 0x002c */ unsigned long Flags;
        struct /* bitfield */
        {
            /* 0x002c */ unsigned char StrictFIFO : 1; /* bit position: 0 */
            /* 0x002c */ unsigned char EnableHandleExceptions : 1; /* bit position: 1 */
            /* 0x002c */ unsigned char Rundown : 1; /* bit position: 2 */
            /* 0x002c */ unsigned char Duplicated : 1; /* bit position: 3 */
            /* 0x002c */ unsigned char RaiseUMExceptionOnInvalidHandleClose : 1; /* bit position: 4 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x0030 */ struct _EX_PUSH_LOCK HandleContentionEvent;
    /* 0x0038 */ struct _EX_PUSH_LOCK HandleTableLock;
    union
    {
        /* 0x0040 */ struct _HANDLE_TABLE_FREE_LIST FreeLists[1];
        struct
        {
            /* 0x0040 */ unsigned char ActualEntry[32];
            /* 0x0060 */ struct _HANDLE_TRACE_DEBUG_INFO* DebugInfo;
            /* 0x0068 */ long __PADDING__[6];
        }; /* size: 0x0040 */
    }; /* size: 0x0040 */
} HANDLE_TABLE, * PHANDLE_TABLE; /* size: 0x0080 */

typedef struct _EPROCESS
{
    /* 0x0000 */ struct _KPROCESS Pcb;
    /* 0x0438 */ struct _EX_PUSH_LOCK ProcessLock;
    /* 0x0440 */ void* UniqueProcessId;
    /* 0x0448 */ struct _LIST_ENTRY ActiveProcessLinks;
    /* 0x0458 */ struct _EX_RUNDOWN_REF RundownProtect;
    union
    {
        /* 0x0460 */ unsigned long Flags2;
        struct /* bitfield */
        {
            /* 0x0460 */ unsigned long JobNotReallyActive : 1; /* bit position: 0 */
            /* 0x0460 */ unsigned long AccountingFolded : 1; /* bit position: 1 */
            /* 0x0460 */ unsigned long NewProcessReported : 1; /* bit position: 2 */
            /* 0x0460 */ unsigned long ExitProcessReported : 1; /* bit position: 3 */
            /* 0x0460 */ unsigned long ReportCommitChanges : 1; /* bit position: 4 */
            /* 0x0460 */ unsigned long LastReportMemory : 1; /* bit position: 5 */
            /* 0x0460 */ unsigned long ForceWakeCharge : 1; /* bit position: 6 */
            /* 0x0460 */ unsigned long CrossSessionCreate : 1; /* bit position: 7 */
            /* 0x0460 */ unsigned long NeedsHandleRundown : 1; /* bit position: 8 */
            /* 0x0460 */ unsigned long RefTraceEnabled : 1; /* bit position: 9 */
            /* 0x0460 */ unsigned long PicoCreated : 1; /* bit position: 10 */
            /* 0x0460 */ unsigned long EmptyJobEvaluated : 1; /* bit position: 11 */
            /* 0x0460 */ unsigned long DefaultPagePriority : 3; /* bit position: 12 */
            /* 0x0460 */ unsigned long PrimaryTokenFrozen : 1; /* bit position: 15 */
            /* 0x0460 */ unsigned long ProcessVerifierTarget : 1; /* bit position: 16 */
            /* 0x0460 */ unsigned long RestrictSetThreadContext : 1; /* bit position: 17 */
            /* 0x0460 */ unsigned long AffinityPermanent : 1; /* bit position: 18 */
            /* 0x0460 */ unsigned long AffinityUpdateEnable : 1; /* bit position: 19 */
            /* 0x0460 */ unsigned long PropagateNode : 1; /* bit position: 20 */
            /* 0x0460 */ unsigned long ExplicitAffinity : 1; /* bit position: 21 */
            /* 0x0460 */ unsigned long ProcessExecutionState : 2; /* bit position: 22 */
            /* 0x0460 */ unsigned long EnableReadVmLogging : 1; /* bit position: 24 */
            /* 0x0460 */ unsigned long EnableWriteVmLogging : 1; /* bit position: 25 */
            /* 0x0460 */ unsigned long FatalAccessTerminationRequested : 1; /* bit position: 26 */
            /* 0x0460 */ unsigned long DisableSystemAllowedCpuSet : 1; /* bit position: 27 */
            /* 0x0460 */ unsigned long ProcessStateChangeRequest : 2; /* bit position: 28 */
            /* 0x0460 */ unsigned long ProcessStateChangeInProgress : 1; /* bit position: 30 */
            /* 0x0460 */ unsigned long InPrivate : 1; /* bit position: 31 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    union
    {
        /* 0x0464 */ unsigned long Flags;
        struct /* bitfield */
        {
            /* 0x0464 */ unsigned long CreateReported : 1; /* bit position: 0 */
            /* 0x0464 */ unsigned long NoDebugInherit : 1; /* bit position: 1 */
            /* 0x0464 */ unsigned long ProcessExiting : 1; /* bit position: 2 */
            /* 0x0464 */ unsigned long ProcessDelete : 1; /* bit position: 3 */
            /* 0x0464 */ unsigned long ManageExecutableMemoryWrites : 1; /* bit position: 4 */
            /* 0x0464 */ unsigned long VmDeleted : 1; /* bit position: 5 */
            /* 0x0464 */ unsigned long OutswapEnabled : 1; /* bit position: 6 */
            /* 0x0464 */ unsigned long Outswapped : 1; /* bit position: 7 */
            /* 0x0464 */ unsigned long FailFastOnCommitFail : 1; /* bit position: 8 */
            /* 0x0464 */ unsigned long Wow64VaSpace4Gb : 1; /* bit position: 9 */
            /* 0x0464 */ unsigned long AddressSpaceInitialized : 2; /* bit position: 10 */
            /* 0x0464 */ unsigned long SetTimerResolution : 1; /* bit position: 12 */
            /* 0x0464 */ unsigned long BreakOnTermination : 1; /* bit position: 13 */
            /* 0x0464 */ unsigned long DeprioritizeViews : 1; /* bit position: 14 */
            /* 0x0464 */ unsigned long WriteWatch : 1; /* bit position: 15 */
            /* 0x0464 */ unsigned long ProcessInSession : 1; /* bit position: 16 */
            /* 0x0464 */ unsigned long OverrideAddressSpace : 1; /* bit position: 17 */
            /* 0x0464 */ unsigned long HasAddressSpace : 1; /* bit position: 18 */
            /* 0x0464 */ unsigned long LaunchPrefetched : 1; /* bit position: 19 */
            /* 0x0464 */ unsigned long Background : 1; /* bit position: 20 */
            /* 0x0464 */ unsigned long VmTopDown : 1; /* bit position: 21 */
            /* 0x0464 */ unsigned long ImageNotifyDone : 1; /* bit position: 22 */
            /* 0x0464 */ unsigned long PdeUpdateNeeded : 1; /* bit position: 23 */
            /* 0x0464 */ unsigned long VdmAllowed : 1; /* bit position: 24 */
            /* 0x0464 */ unsigned long ProcessRundown : 1; /* bit position: 25 */
            /* 0x0464 */ unsigned long ProcessInserted : 1; /* bit position: 26 */
            /* 0x0464 */ unsigned long DefaultIoPriority : 3; /* bit position: 27 */
            /* 0x0464 */ unsigned long ProcessSelfDelete : 1; /* bit position: 30 */
            /* 0x0464 */ unsigned long SetTimerResolutionLink : 1; /* bit position: 31 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x0468 */ union _LARGE_INTEGER CreateTime;
    /* 0x0470 */ unsigned __int64 ProcessQuotaUsage[2];
    /* 0x0480 */ unsigned __int64 ProcessQuotaPeak[2];
    /* 0x0490 */ unsigned __int64 PeakVirtualSize;
    /* 0x0498 */ unsigned __int64 VirtualSize;
    /* 0x04a0 */ struct _LIST_ENTRY SessionProcessLinks;
    union
    {
        /* 0x04b0 */ void* ExceptionPortData;
        /* 0x04b0 */ unsigned __int64 ExceptionPortValue;
        /* 0x04b0 */ unsigned __int64 ExceptionPortState : 3; /* bit position: 0 */
    }; /* size: 0x0008 */
    /* 0x04b8 */ struct _EX_FAST_REF Token;
    /* 0x04c0 */ unsigned __int64 MmReserved;
    /* 0x04c8 */ struct _EX_PUSH_LOCK AddressCreationLock;
    /* 0x04d0 */ struct _EX_PUSH_LOCK PageTableCommitmentLock;
    /* 0x04d8 */ struct _ETHREAD* RotateInProgress;
    /* 0x04e0 */ struct _ETHREAD* ForkInProgress;
    /* 0x04e8 */ struct _EJOB* volatile CommitChargeJob;
    /* 0x04f0 */ struct _RTL_AVL_TREE CloneRoot;
    /* 0x04f8 */ volatile unsigned __int64 NumberOfPrivatePages;
    /* 0x0500 */ volatile unsigned __int64 NumberOfLockedPages;
    /* 0x0508 */ void* Win32Process;
    /* 0x0510 */ struct _EJOB* volatile Job;
    /* 0x0518 */ void* SectionObject;
    /* 0x0520 */ void* SectionBaseAddress;
    /* 0x0528 */ unsigned long Cookie;
    /* 0x052c */ long Padding_156;
    /* 0x0530 */ struct _PAGEFAULT_HISTORY* WorkingSetWatch;
    /* 0x0538 */ void* Win32WindowStation;
    /* 0x0540 */ void* InheritedFromUniqueProcessId;
    /* 0x0548 */ volatile unsigned __int64 OwnerProcessId;
    /* 0x0550 */ struct _PEB* Peb;
    /* 0x0558 */ struct _MM_SESSION_SPACE* Session;
    /* 0x0560 */ void* Spare1;
    /* 0x0568 */ struct _EPROCESS_QUOTA_BLOCK* QuotaBlock;
    /* 0x0570 */ struct _HANDLE_TABLE* ObjectTable;
    /* 0x0578 */ void* DebugPort;
    /* 0x0580 */ struct _EWOW64PROCESS* WoW64Process;
    /* 0x0588 */ void* DeviceMap;
    /* 0x0590 */ void* EtwDataSource;
    /* 0x0598 */ unsigned __int64 PageDirectoryPte;
    /* 0x05a0 */ struct _FILE_OBJECT* ImageFilePointer;
    /* 0x05a8 */ unsigned char ImageFileName[15];
    /* 0x05b7 */ unsigned char PriorityClass;
    /* 0x05b8 */ void* SecurityPort;
    /* 0x05c0 */ struct _SE_AUDIT_PROCESS_CREATION_INFO SeAuditProcessCreationInfo;
    /* 0x05c8 */ struct _LIST_ENTRY JobLinks;
    /* 0x05d8 */ void* HighestUserAddress;
    /* 0x05e0 */ struct _LIST_ENTRY ThreadListHead;
    /* 0x05f0 */ volatile unsigned long ActiveThreads;
    /* 0x05f4 */ unsigned long ImagePathHash;
    /* 0x05f8 */ unsigned long DefaultHardErrorProcessing;
    /* 0x05fc */ long LastThreadExitStatus;
    /* 0x0600 */ struct _EX_FAST_REF PrefetchTrace;
    /* 0x0608 */ void* LockedPagesList;
    /* 0x0610 */ union _LARGE_INTEGER ReadOperationCount;
    /* 0x0618 */ union _LARGE_INTEGER WriteOperationCount;
    /* 0x0620 */ union _LARGE_INTEGER OtherOperationCount;
    /* 0x0628 */ union _LARGE_INTEGER ReadTransferCount;
    /* 0x0630 */ union _LARGE_INTEGER WriteTransferCount;
    /* 0x0638 */ union _LARGE_INTEGER OtherTransferCount;
    /* 0x0640 */ unsigned __int64 CommitChargeLimit;
    /* 0x0648 */ volatile unsigned __int64 CommitCharge;
    /* 0x0650 */ volatile unsigned __int64 CommitChargePeak;
    /* 0x0658 */ long Padding_157[10];
    /* 0x0680 */ struct _MMSUPPORT_FULL Vm;
    /* 0x07c0 */ struct _LIST_ENTRY MmProcessLinks;
    /* 0x07d0 */ unsigned long ModifiedPageCount;
    /* 0x07d4 */ long ExitStatus;
    /* 0x07d8 */ struct _RTL_AVL_TREE VadRoot;
    /* 0x07e0 */ void* VadHint;
    /* 0x07e8 */ unsigned __int64 VadCount;
    /* 0x07f0 */ volatile unsigned __int64 VadPhysicalPages;
    /* 0x07f8 */ unsigned __int64 VadPhysicalPagesLimit;
    /* 0x0800 */ struct _ALPC_PROCESS_CONTEXT AlpcContext;
    /* 0x0820 */ struct _LIST_ENTRY TimerResolutionLink;
    /* 0x0830 */ struct _PO_DIAG_STACK_RECORD* TimerResolutionStackRecord;
    /* 0x0838 */ unsigned long RequestedTimerResolution;
    /* 0x083c */ unsigned long SmallestTimerResolution;
    /* 0x0840 */ union _LARGE_INTEGER ExitTime;
    /* 0x0848 */ struct _INVERTED_FUNCTION_TABLE* InvertedFunctionTable;
    /* 0x0850 */ struct _EX_PUSH_LOCK InvertedFunctionTableLock;
    /* 0x0858 */ unsigned long ActiveThreadsHighWatermark;
    /* 0x085c */ unsigned long LargePrivateVadCount;
    /* 0x0860 */ struct _EX_PUSH_LOCK ThreadListLock;
    /* 0x0868 */ void* WnfContext;
    /* 0x0870 */ struct _EJOB* ServerSilo;
    /* 0x0878 */ unsigned char SignatureLevel;
    /* 0x0879 */ unsigned char SectionSignatureLevel;
    /* 0x087a */ struct _PS_PROTECTION Protection;
    struct /* bitfield */
    {
        /* 0x087b */ unsigned char HangCount : 3; /* bit position: 0 */
        /* 0x087b */ unsigned char GhostCount : 3; /* bit position: 3 */
        /* 0x087b */ unsigned char PrefilterException : 1; /* bit position: 6 */
    }; /* bitfield */
    union
    {
        /* 0x087c */ unsigned long Flags3;
        struct /* bitfield */
        {
            /* 0x087c */ unsigned long Minimal : 1; /* bit position: 0 */
            /* 0x087c */ unsigned long ReplacingPageRoot : 1; /* bit position: 1 */
            /* 0x087c */ unsigned long Crashed : 1; /* bit position: 2 */
            /* 0x087c */ unsigned long JobVadsAreTracked : 1; /* bit position: 3 */
            /* 0x087c */ unsigned long VadTrackingDisabled : 1; /* bit position: 4 */
            /* 0x087c */ unsigned long AuxiliaryProcess : 1; /* bit position: 5 */
            /* 0x087c */ unsigned long SubsystemProcess : 1; /* bit position: 6 */
            /* 0x087c */ unsigned long IndirectCpuSets : 1; /* bit position: 7 */
            /* 0x087c */ unsigned long RelinquishedCommit : 1; /* bit position: 8 */
            /* 0x087c */ unsigned long HighGraphicsPriority : 1; /* bit position: 9 */
            /* 0x087c */ unsigned long CommitFailLogged : 1; /* bit position: 10 */
            /* 0x087c */ unsigned long ReserveFailLogged : 1; /* bit position: 11 */
            /* 0x087c */ unsigned long SystemProcess : 1; /* bit position: 12 */
            /* 0x087c */ unsigned long HideImageBaseAddresses : 1; /* bit position: 13 */
            /* 0x087c */ unsigned long AddressPolicyFrozen : 1; /* bit position: 14 */
            /* 0x087c */ unsigned long ProcessFirstResume : 1; /* bit position: 15 */
            /* 0x087c */ unsigned long ForegroundExternal : 1; /* bit position: 16 */
            /* 0x087c */ unsigned long ForegroundSystem : 1; /* bit position: 17 */
            /* 0x087c */ unsigned long HighMemoryPriority : 1; /* bit position: 18 */
            /* 0x087c */ unsigned long EnableProcessSuspendResumeLogging : 1; /* bit position: 19 */
            /* 0x087c */ unsigned long EnableThreadSuspendResumeLogging : 1; /* bit position: 20 */
            /* 0x087c */ unsigned long SecurityDomainChanged : 1; /* bit position: 21 */
            /* 0x087c */ unsigned long SecurityFreezeComplete : 1; /* bit position: 22 */
            /* 0x087c */ unsigned long VmProcessorHost : 1; /* bit position: 23 */
            /* 0x087c */ unsigned long VmProcessorHostTransition : 1; /* bit position: 24 */
            /* 0x087c */ unsigned long AltSyscall : 1; /* bit position: 25 */
            /* 0x087c */ unsigned long TimerResolutionIgnore : 1; /* bit position: 26 */
            /* 0x087c */ unsigned long DisallowUserTerminate : 1; /* bit position: 27 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x0880 */ long DeviceAsid;
    /* 0x0884 */ long Padding_158;
    /* 0x0888 */ void* SvmData;
    /* 0x0890 */ struct _EX_PUSH_LOCK SvmProcessLock;
    /* 0x0898 */ unsigned __int64 SvmLock;
    /* 0x08a0 */ struct _LIST_ENTRY SvmProcessDeviceListHead;
    /* 0x08b0 */ unsigned __int64 LastFreezeInterruptTime;
    /* 0x08b8 */ struct _PROCESS_DISK_COUNTERS* DiskCounters;
    /* 0x08c0 */ void* PicoContext;
    /* 0x08c8 */ void* EnclaveTable;
    /* 0x08d0 */ unsigned __int64 EnclaveNumber;
    /* 0x08d8 */ struct _EX_PUSH_LOCK EnclaveLock;
    /* 0x08e0 */ unsigned long HighPriorityFaultsAllowed;
    /* 0x08e4 */ long Padding_159;
    /* 0x08e8 */ struct _PO_PROCESS_ENERGY_CONTEXT* EnergyContext;
    /* 0x08f0 */ void* VmContext;
    /* 0x08f8 */ unsigned __int64 SequenceNumber;
    /* 0x0900 */ unsigned __int64 CreateInterruptTime;
    /* 0x0908 */ unsigned __int64 CreateUnbiasedInterruptTime;
    /* 0x0910 */ unsigned __int64 TotalUnbiasedFrozenTime;
    /* 0x0918 */ unsigned __int64 LastAppStateUpdateTime;
    struct /* bitfield */
    {
        /* 0x0920 */ unsigned __int64 LastAppStateUptime : 61; /* bit position: 0 */
        /* 0x0920 */ unsigned __int64 LastAppState : 3; /* bit position: 61 */
    }; /* bitfield */
    /* 0x0928 */ volatile unsigned __int64 SharedCommitCharge;
    /* 0x0930 */ struct _EX_PUSH_LOCK SharedCommitLock;
    /* 0x0938 */ struct _LIST_ENTRY SharedCommitLinks;
    union
    {
        struct
        {
            /* 0x0948 */ unsigned __int64 AllowedCpuSets;
            /* 0x0950 */ unsigned __int64 DefaultCpuSets;
        }; /* size: 0x0010 */
        struct
        {
            /* 0x0948 */ unsigned __int64* AllowedCpuSetsIndirect;
            /* 0x0950 */ unsigned __int64* DefaultCpuSetsIndirect;
        }; /* size: 0x0010 */
    }; /* size: 0x0010 */
    /* 0x0958 */ void* DiskIoAttribution;
    /* 0x0960 */ void* DxgProcess;
    /* 0x0968 */ unsigned long Win32KFilterSet;
    /* 0x096c */ long Padding_160;
    /* 0x0970 */ volatile union _PS_INTERLOCKED_TIMER_DELAY_VALUES ProcessTimerDelay;
    /* 0x0978 */ volatile unsigned long KTimerSets;
    /* 0x097c */ volatile unsigned long KTimer2Sets;
    /* 0x0980 */ volatile unsigned long ThreadTimerSets;
    /* 0x0984 */ long Padding_161;
    /* 0x0988 */ unsigned __int64 VirtualTimerListLock;
    /* 0x0990 */ struct _LIST_ENTRY VirtualTimerListHead;
    union
    {
        /* 0x09a0 */ struct _WNF_STATE_NAME WakeChannel;
        /* 0x09a0 */ struct _PS_PROCESS_WAKE_INFORMATION WakeInfo;
    }; /* size: 0x0030 */
    union
    {
        /* 0x09d0 */ unsigned long MitigationFlags;
        struct
        {
            struct /* bitfield */
            {
                /* 0x09d0 */ unsigned long ControlFlowGuardEnabled : 1; /* bit position: 0 */
                /* 0x09d0 */ unsigned long ControlFlowGuardExportSuppressionEnabled : 1; /* bit position: 1 */
                /* 0x09d0 */ unsigned long ControlFlowGuardStrict : 1; /* bit position: 2 */
                /* 0x09d0 */ unsigned long DisallowStrippedImages : 1; /* bit position: 3 */
                /* 0x09d0 */ unsigned long ForceRelocateImages : 1; /* bit position: 4 */
                /* 0x09d0 */ unsigned long HighEntropyASLREnabled : 1; /* bit position: 5 */
                /* 0x09d0 */ unsigned long StackRandomizationDisabled : 1; /* bit position: 6 */
                /* 0x09d0 */ unsigned long ExtensionPointDisable : 1; /* bit position: 7 */
                /* 0x09d0 */ unsigned long DisableDynamicCode : 1; /* bit position: 8 */
                /* 0x09d0 */ unsigned long DisableDynamicCodeAllowOptOut : 1; /* bit position: 9 */
                /* 0x09d0 */ unsigned long DisableDynamicCodeAllowRemoteDowngrade : 1; /* bit position: 10 */
                /* 0x09d0 */ unsigned long AuditDisableDynamicCode : 1; /* bit position: 11 */
                /* 0x09d0 */ unsigned long DisallowWin32kSystemCalls : 1; /* bit position: 12 */
                /* 0x09d0 */ unsigned long AuditDisallowWin32kSystemCalls : 1; /* bit position: 13 */
                /* 0x09d0 */ unsigned long EnableFilteredWin32kAPIs : 1; /* bit position: 14 */
                /* 0x09d0 */ unsigned long AuditFilteredWin32kAPIs : 1; /* bit position: 15 */
                /* 0x09d0 */ unsigned long DisableNonSystemFonts : 1; /* bit position: 16 */
                /* 0x09d0 */ unsigned long AuditNonSystemFontLoading : 1; /* bit position: 17 */
                /* 0x09d0 */ unsigned long PreferSystem32Images : 1; /* bit position: 18 */
                /* 0x09d0 */ unsigned long ProhibitRemoteImageMap : 1; /* bit position: 19 */
                /* 0x09d0 */ unsigned long AuditProhibitRemoteImageMap : 1; /* bit position: 20 */
                /* 0x09d0 */ unsigned long ProhibitLowILImageMap : 1; /* bit position: 21 */
                /* 0x09d0 */ unsigned long AuditProhibitLowILImageMap : 1; /* bit position: 22 */
                /* 0x09d0 */ unsigned long SignatureMitigationOptIn : 1; /* bit position: 23 */
                /* 0x09d0 */ unsigned long AuditBlockNonMicrosoftBinaries : 1; /* bit position: 24 */
                /* 0x09d0 */ unsigned long AuditBlockNonMicrosoftBinariesAllowStore : 1; /* bit position: 25 */
                /* 0x09d0 */ unsigned long LoaderIntegrityContinuityEnabled : 1; /* bit position: 26 */
                /* 0x09d0 */ unsigned long AuditLoaderIntegrityContinuity : 1; /* bit position: 27 */
                /* 0x09d0 */ unsigned long EnableModuleTamperingProtection : 1; /* bit position: 28 */
                /* 0x09d0 */ unsigned long EnableModuleTamperingProtectionNoInherit : 1; /* bit position: 29 */
                /* 0x09d0 */ unsigned long RestrictIndirectBranchPrediction : 1; /* bit position: 30 */
                /* 0x09d0 */ unsigned long IsolateSecurityDomain : 1; /* bit position: 31 */
            }; /* bitfield */
        } /* size: 0x0004 */ MitigationFlagsValues;
    }; /* size: 0x0004 */
    union
    {
        /* 0x09d4 */ unsigned long MitigationFlags2;
        struct
        {
            struct /* bitfield */
            {
                /* 0x09d4 */ unsigned long EnableExportAddressFilter : 1; /* bit position: 0 */
                /* 0x09d4 */ unsigned long AuditExportAddressFilter : 1; /* bit position: 1 */
                /* 0x09d4 */ unsigned long EnableExportAddressFilterPlus : 1; /* bit position: 2 */
                /* 0x09d4 */ unsigned long AuditExportAddressFilterPlus : 1; /* bit position: 3 */
                /* 0x09d4 */ unsigned long EnableRopStackPivot : 1; /* bit position: 4 */
                /* 0x09d4 */ unsigned long AuditRopStackPivot : 1; /* bit position: 5 */
                /* 0x09d4 */ unsigned long EnableRopCallerCheck : 1; /* bit position: 6 */
                /* 0x09d4 */ unsigned long AuditRopCallerCheck : 1; /* bit position: 7 */
                /* 0x09d4 */ unsigned long EnableRopSimExec : 1; /* bit position: 8 */
                /* 0x09d4 */ unsigned long AuditRopSimExec : 1; /* bit position: 9 */
                /* 0x09d4 */ unsigned long EnableImportAddressFilter : 1; /* bit position: 10 */
                /* 0x09d4 */ unsigned long AuditImportAddressFilter : 1; /* bit position: 11 */
                /* 0x09d4 */ unsigned long DisablePageCombine : 1; /* bit position: 12 */
                /* 0x09d4 */ unsigned long SpeculativeStoreBypassDisable : 1; /* bit position: 13 */
                /* 0x09d4 */ unsigned long CetUserShadowStacks : 1; /* bit position: 14 */
                /* 0x09d4 */ unsigned long AuditCetUserShadowStacks : 1; /* bit position: 15 */
                /* 0x09d4 */ unsigned long AuditCetUserShadowStacksLogged : 1; /* bit position: 16 */
                /* 0x09d4 */ unsigned long UserCetSetContextIpValidation : 1; /* bit position: 17 */
                /* 0x09d4 */ unsigned long AuditUserCetSetContextIpValidation : 1; /* bit position: 18 */
                /* 0x09d4 */ unsigned long AuditUserCetSetContextIpValidationLogged : 1; /* bit position: 19 */
                /* 0x09d4 */ unsigned long CetUserShadowStacksStrictMode : 1; /* bit position: 20 */
                /* 0x09d4 */ unsigned long BlockNonCetBinaries : 1; /* bit position: 21 */
                /* 0x09d4 */ unsigned long BlockNonCetBinariesNonEhcont : 1; /* bit position: 22 */
                /* 0x09d4 */ unsigned long AuditBlockNonCetBinaries : 1; /* bit position: 23 */
                /* 0x09d4 */ unsigned long AuditBlockNonCetBinariesLogged : 1; /* bit position: 24 */
                /* 0x09d4 */ unsigned long Reserved1 : 1; /* bit position: 25 */
                /* 0x09d4 */ unsigned long Reserved2 : 1; /* bit position: 26 */
                /* 0x09d4 */ unsigned long Reserved3 : 1; /* bit position: 27 */
                /* 0x09d4 */ unsigned long Reserved4 : 1; /* bit position: 28 */
                /* 0x09d4 */ unsigned long Reserved5 : 1; /* bit position: 29 */
                /* 0x09d4 */ unsigned long CetDynamicApisOutOfProcOnly : 1; /* bit position: 30 */
                /* 0x09d4 */ unsigned long UserCetSetContextIpValidationRelaxedMode : 1; /* bit position: 31 */
            }; /* bitfield */
        } /* size: 0x0004 */ MitigationFlags2Values;
    }; /* size: 0x0004 */
    /* 0x09d8 */ void* PartitionObject;
    /* 0x09e0 */ unsigned __int64 SecurityDomain;
    /* 0x09e8 */ unsigned __int64 ParentSecurityDomain;
    /* 0x09f0 */ void* CoverageSamplerContext;
    /* 0x09f8 */ void* MmHotPatchContext;
    /* 0x0a00 */ struct _RTL_AVL_TREE DynamicEHContinuationTargetsTree;
    /* 0x0a08 */ struct _EX_PUSH_LOCK DynamicEHContinuationTargetsLock;
    /* 0x0a10 */ struct _PS_DYNAMIC_ENFORCED_ADDRESS_RANGES DynamicEnforcedCetCompatibleRanges;
    /* 0x0a20 */ unsigned long DisabledComponentFlags;
    /* 0x0a24 */ long Padding_162;
    /* 0x0a28 */ unsigned long* volatile PathRedirectionHashes;
    union
    {
        /* 0x0a30 */ unsigned long MitigationFlags3;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0a30 */ unsigned long RestrictCoreSharing : 1; /* bit position: 0 */
                /* 0x0a30 */ unsigned long DisallowFsctlSystemCalls : 1; /* bit position: 1 */
                /* 0x0a30 */ unsigned long AuditDisallowFsctlSystemCalls : 1; /* bit position: 2 */
                /* 0x0a30 */ unsigned long MitigationFlags3Spare : 29; /* bit position: 3 */
            }; /* bitfield */
        } /* size: 0x0004 */ MitigationFlags3Values;
    }; /* size: 0x0004 */
    /* 0x0a34 */ long __PADDING__[3];
} EPROCESS, * PMY_EPROCESS; /* size: 0x0a40 */

typedef struct _PSP_SYSTEM_DLL
{
    /* 0x0000 */ struct _EX_FAST_REF DllSection;
    /* 0x0008 */ struct _EX_PUSH_LOCK DllLock;
} PSP_SYSTEM_DLL, * PPSP_SYSTEM_DLL; /* size: 0x0010 */


typedef struct _OBJECT_TYPE
{
    /* 0x0000 */ struct _LIST_ENTRY TypeList;
    /* 0x0010 */ struct _UNICODE_STRING Name;
    /* 0x0020 */ void* DefaultObject;
    /* 0x0028 */ unsigned char Index;
    /* 0x0029 */ char Padding_425[3];
    /* 0x002c */ unsigned long TotalNumberOfObjects;
    /* 0x0030 */ unsigned long TotalNumberOfHandles;
    /* 0x0034 */ unsigned long HighWaterNumberOfObjects;
    /* 0x0038 */ unsigned long HighWaterNumberOfHandles;
    /* 0x003c */ long Padding_426;
    /* 0x0040 */ struct _OBJECT_TYPE_INITIALIZER TypeInfo;
    /* 0x00b8 */ struct _EX_PUSH_LOCK TypeLock;
    /* 0x00c0 */ unsigned long Key;
    /* 0x00c4 */ long Padding_427;
    /* 0x00c8 */ struct _LIST_ENTRY CallbackList;
} OBJECT_TYPE, * POBJECT_TYPE; /* size: 0x00d8 */

typedef struct _PS_SYSTEM_DLL_INFO
{
    union
    {
        /* 0x0000 */ unsigned long Flags;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned long Required : 1; /* bit position: 0 */
            /* 0x0000 */ unsigned long VerifyImage : 1; /* bit position: 1 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x0004 */ long Padding_151;
    /* 0x0008 */ struct _UNICODE_STRING DllPath;
    /* 0x0018 */ void* BaseAddress;
    /* 0x0020 */ unsigned short* DbgName;
} PS_SYSTEM_DLL_INFO, * PPS_SYSTEM_DLL_INFO; /* size: 0x0028 */

typedef struct _PSP_SYSTEM_DLL_DATA
{
    /* 0x0000 */ struct _PSP_SYSTEM_DLL* SystemDll;
    /* 0x0008 */ struct _PS_SYSTEM_DLL_INFO DllInfo;
} PSP_SYSTEM_DLL_DATA, * PPSP_SYSTEM_DLL_DATA; /* size: 0x0038 */

typedef struct _EWOW64PROCESS
{
    /* 0x0000 */ void* Peb;
    /* 0x0008 */ unsigned short Machine;
    /* 0x000a */ char Padding_182[2];
    /* 0x000c */ enum _SYSTEM_DLL_TYPE NtdllType;
} EWOW64PROCESS, * PEWOW64PROCESS; /* size: 0x0010 */

//
//typedef struct _PEB32
//{
//    /* 0x0000 */ unsigned char InheritedAddressSpace;
//    /* 0x0001 */ unsigned char ReadImageFileExecOptions;
//    /* 0x0002 */ unsigned char BeingDebugged;
//    union
//    {
//        /* 0x0003 */ unsigned char BitField;
//        struct /* bitfield */
//        {
//            /* 0x0003 */ unsigned char ImageUsesLargePages : 1; /* bit position: 0 */
//            /* 0x0003 */ unsigned char IsProtectedProcess : 1; /* bit position: 1 */
//            /* 0x0003 */ unsigned char IsImageDynamicallyRelocated : 1; /* bit position: 2 */
//            /* 0x0003 */ unsigned char SkipPatchingUser32Forwarders : 1; /* bit position: 3 */
//            /* 0x0003 */ unsigned char IsPackagedProcess : 1; /* bit position: 4 */
//            /* 0x0003 */ unsigned char IsAppContainer : 1; /* bit position: 5 */
//            /* 0x0003 */ unsigned char IsProtectedProcessLight : 1; /* bit position: 6 */
//            /* 0x0003 */ unsigned char IsLongPathAwareProcess : 1; /* bit position: 7 */
//        }; /* bitfield */
//    }; /* size: 0x0001 */
//    /* 0x0004 */ unsigned long Mutant;
//    /* 0x0008 */ unsigned long ImageBaseAddress;
//    /* 0x000c */ unsigned long Ldr;
//    /* 0x0010 */ unsigned long ProcessParameters;
//    /* 0x0014 */ unsigned long SubSystemData;
//    /* 0x0018 */ unsigned long ProcessHeap;
//    /* 0x001c */ unsigned long FastPebLock;
//    /* 0x0020 */ unsigned long AtlThunkSListPtr;
//    /* 0x0024 */ unsigned long IFEOKey;
//    union
//    {
//        /* 0x0028 */ unsigned long CrossProcessFlags;
//        struct /* bitfield */
//        {
//            /* 0x0028 */ unsigned long ProcessInJob : 1; /* bit position: 0 */
//            /* 0x0028 */ unsigned long ProcessInitializing : 1; /* bit position: 1 */
//            /* 0x0028 */ unsigned long ProcessUsingVEH : 1; /* bit position: 2 */
//            /* 0x0028 */ unsigned long ProcessUsingVCH : 1; /* bit position: 3 */
//            /* 0x0028 */ unsigned long ProcessUsingFTH : 1; /* bit position: 4 */
//            /* 0x0028 */ unsigned long ProcessPreviouslyThrottled : 1; /* bit position: 5 */
//            /* 0x0028 */ unsigned long ProcessCurrentlyThrottled : 1; /* bit position: 6 */
//            /* 0x0028 */ unsigned long ProcessImagesHotPatched : 1; /* bit position: 7 */
//            /* 0x0028 */ unsigned long ReservedBits0 : 24; /* bit position: 8 */
//        }; /* bitfield */
//    }; /* size: 0x0004 */
//    union
//    {
//        /* 0x002c */ unsigned long KernelCallbackTable;
//        /* 0x002c */ unsigned long UserSharedInfoPtr;
//    }; /* size: 0x0004 */
//    /* 0x0030 */ unsigned long SystemReserved;
//    /* 0x0034 */ unsigned long AtlThunkSListPtr32;
//    /* 0x0038 */ unsigned long ApiSetMap;
//    /* 0x003c */ unsigned long TlsExpansionCounter;
//    /* 0x0040 */ unsigned long TlsBitmap;
//    /* 0x0044 */ unsigned long TlsBitmapBits[2];
//    /* 0x004c */ unsigned long ReadOnlySharedMemoryBase;
//    /* 0x0050 */ unsigned long SharedData;
//    /* 0x0054 */ unsigned long ReadOnlyStaticServerData;
//    /* 0x0058 */ unsigned long AnsiCodePageData;
//    /* 0x005c */ unsigned long OemCodePageData;
//    /* 0x0060 */ unsigned long UnicodeCaseTableData;
//    /* 0x0064 */ unsigned long NumberOfProcessors;
//    /* 0x0068 */ unsigned long NtGlobalFlag;
//    /* 0x006c */ long Padding_645;
//    /* 0x0070 */ union _LARGE_INTEGER CriticalSectionTimeout;
//    /* 0x0078 */ unsigned long HeapSegmentReserve;
//    /* 0x007c */ unsigned long HeapSegmentCommit;
//    /* 0x0080 */ unsigned long HeapDeCommitTotalFreeThreshold;
//    /* 0x0084 */ unsigned long HeapDeCommitFreeBlockThreshold;
//    /* 0x0088 */ unsigned long NumberOfHeaps;
//    /* 0x008c */ unsigned long MaximumNumberOfHeaps;
//    /* 0x0090 */ unsigned long ProcessHeaps;
//    /* 0x0094 */ unsigned long GdiSharedHandleTable;
//    /* 0x0098 */ unsigned long ProcessStarterHelper;
//    /* 0x009c */ unsigned long GdiDCAttributeList;
//    /* 0x00a0 */ unsigned long LoaderLock;
//    /* 0x00a4 */ unsigned long OSMajorVersion;
//    /* 0x00a8 */ unsigned long OSMinorVersion;
//    /* 0x00ac */ unsigned short OSBuildNumber;
//    /* 0x00ae */ unsigned short OSCSDVersion;
//    /* 0x00b0 */ unsigned long OSPlatformId;
//    /* 0x00b4 */ unsigned long ImageSubsystem;
//    /* 0x00b8 */ unsigned long ImageSubsystemMajorVersion;
//    /* 0x00bc */ unsigned long ImageSubsystemMinorVersion;
//    /* 0x00c0 */ unsigned long ActiveProcessAffinityMask;
//    /* 0x00c4 */ unsigned long GdiHandleBuffer[34];
//    /* 0x014c */ unsigned long PostProcessInitRoutine;
//    /* 0x0150 */ unsigned long TlsExpansionBitmap;
//    /* 0x0154 */ unsigned long TlsExpansionBitmapBits[32];
//    /* 0x01d4 */ unsigned long SessionId;
//    /* 0x01d8 */ union _ULARGE_INTEGER AppCompatFlags;
//    /* 0x01e0 */ union _ULARGE_INTEGER AppCompatFlagsUser;
//    /* 0x01e8 */ unsigned long pShimData;
//    /* 0x01ec */ unsigned long AppCompatInfo;
//    /* 0x01f0 */ struct _STRING32 CSDVersion;
//    /* 0x01f8 */ unsigned long ActivationContextData;
//    /* 0x01fc */ unsigned long ProcessAssemblyStorageMap;
//    /* 0x0200 */ unsigned long SystemDefaultActivationContextData;
//    /* 0x0204 */ unsigned long SystemAssemblyStorageMap;
//    /* 0x0208 */ unsigned long MinimumStackCommit;
//    /* 0x020c */ unsigned long SparePointers[4];
//    /* 0x021c */ unsigned long SpareUlongs[5];
//    /* 0x0230 */ unsigned long WerRegistrationData;
//    /* 0x0234 */ unsigned long WerShipAssertPtr;
//    /* 0x0238 */ unsigned long pUnused;
//    /* 0x023c */ unsigned long pImageHeaderHash;
//    union
//    {
//        /* 0x0240 */ unsigned long TracingFlags;
//        struct /* bitfield */
//        {
//            /* 0x0240 */ unsigned long HeapTracingEnabled : 1; /* bit position: 0 */
//            /* 0x0240 */ unsigned long CritSecTracingEnabled : 1; /* bit position: 1 */
//            /* 0x0240 */ unsigned long LibLoaderTracingEnabled : 1; /* bit position: 2 */
//            /* 0x0240 */ unsigned long SpareTracingBits : 29; /* bit position: 3 */
//        }; /* bitfield */
//    }; /* size: 0x0004 */
//    /* 0x0244 */ long Padding_646;
//    /* 0x0248 */ unsigned __int64 CsrServerReadOnlySharedMemoryBase;
//    /* 0x0250 */ unsigned long TppWorkerpListLock;
//    /* 0x0254 */ struct LIST_ENTRY32 TppWorkerpList;
//    /* 0x025c */ unsigned long WaitOnAddressHashTable[128];
//    /* 0x045c */ unsigned long TelemetryCoverageHeader;
//    /* 0x0460 */ unsigned long CloudFileFlags;
//    /* 0x0464 */ unsigned long CloudFileDiagFlags;
//    /* 0x0468 */ char PlaceholderCompatibilityMode;
//    /* 0x0469 */ char PlaceholderCompatibilityModeReserved[7];
//    /* 0x0470 */ unsigned long LeapSecondData;
//    union
//    {
//        /* 0x0474 */ unsigned long LeapSecondFlags;
//        struct /* bitfield */
//        {
//            /* 0x0474 */ unsigned long SixtySecondEnabled : 1; /* bit position: 0 */
//            /* 0x0474 */ unsigned long Reserved : 31; /* bit position: 1 */
//        }; /* bitfield */
//    }; /* size: 0x0004 */
//    /* 0x0478 */ unsigned long NtGlobalFlag2;
//    /* 0x047c */ long __PADDING__[1];
//} PEB32, * PPEB32; /* size: 0x0480 */
//
//typedef struct _PEB_LDR_DATA
//{
//    /* 0x0000 */ unsigned long Length;
//    /* 0x0004 */ unsigned char Initialized;
//    /* 0x0005 */ char Padding_342[3];
//    /* 0x0008 */ void* SsHandle;
//    /* 0x0010 */ struct _LIST_ENTRY InLoadOrderModuleList;
//    /* 0x0020 */ struct _LIST_ENTRY InMemoryOrderModuleList;
//    /* 0x0030 */ struct _LIST_ENTRY InInitializationOrderModuleList;
//    /* 0x0040 */ void* EntryInProgress;
//    /* 0x0048 */ unsigned char ShutdownInProgress;
//    /* 0x0049 */ char Padding_343[7];
//    /* 0x0050 */ void* ShutdownThreadId;
//} PEB_LDR_DATA, * PPEB_LDR_DATA; /* size: 0x0058 */

//
//typedef struct _LDR_DATA_TABLE_ENTRY
//{
//    /* 0x0000 */ struct _LIST_ENTRY InLoadOrderLinks;
//    /* 0x0010 */ struct _LIST_ENTRY InMemoryOrderLinks;
//    /* 0x0020 */ struct _LIST_ENTRY InInitializationOrderLinks;
//    /* 0x0030 */ void* DllBase;
//    /* 0x0038 */ void* EntryPoint;
//    /* 0x0040 */ unsigned long SizeOfImage;
//    /* 0x0044 */ long Padding_769;
//    /* 0x0048 */ struct _UNICODE_STRING FullDllName;
//    /* 0x0058 */ struct _UNICODE_STRING BaseDllName;
//    union
//    {
//        /* 0x0068 */ unsigned char FlagGroup[4];
//        /* 0x0068 */ unsigned long Flags;
//        struct /* bitfield */
//        {
//            /* 0x0068 */ unsigned long PackagedBinary : 1; /* bit position: 0 */
//            /* 0x0068 */ unsigned long MarkedForRemoval : 1; /* bit position: 1 */
//            /* 0x0068 */ unsigned long ImageDll : 1; /* bit position: 2 */
//            /* 0x0068 */ unsigned long LoadNotificationsSent : 1; /* bit position: 3 */
//            /* 0x0068 */ unsigned long TelemetryEntryProcessed : 1; /* bit position: 4 */
//            /* 0x0068 */ unsigned long ProcessStaticImport : 1; /* bit position: 5 */
//            /* 0x0068 */ unsigned long InLegacyLists : 1; /* bit position: 6 */
//            /* 0x0068 */ unsigned long InIndexes : 1; /* bit position: 7 */
//            /* 0x0068 */ unsigned long ShimDll : 1; /* bit position: 8 */
//            /* 0x0068 */ unsigned long InExceptionTable : 1; /* bit position: 9 */
//            /* 0x0068 */ unsigned long ReservedFlags1 : 2; /* bit position: 10 */
//            /* 0x0068 */ unsigned long LoadInProgress : 1; /* bit position: 12 */
//            /* 0x0068 */ unsigned long LoadConfigProcessed : 1; /* bit position: 13 */
//            /* 0x0068 */ unsigned long EntryProcessed : 1; /* bit position: 14 */
//            /* 0x0068 */ unsigned long ProtectDelayLoad : 1; /* bit position: 15 */
//            /* 0x0068 */ unsigned long ReservedFlags3 : 2; /* bit position: 16 */
//            /* 0x0068 */ unsigned long DontCallForThreads : 1; /* bit position: 18 */
//            /* 0x0068 */ unsigned long ProcessAttachCalled : 1; /* bit position: 19 */
//            /* 0x0068 */ unsigned long ProcessAttachFailed : 1; /* bit position: 20 */
//            /* 0x0068 */ unsigned long CorDeferredValidate : 1; /* bit position: 21 */
//            /* 0x0068 */ unsigned long CorImage : 1; /* bit position: 22 */
//            /* 0x0068 */ unsigned long DontRelocate : 1; /* bit position: 23 */
//            /* 0x0068 */ unsigned long CorILOnly : 1; /* bit position: 24 */
//            /* 0x0068 */ unsigned long ChpeImage : 1; /* bit position: 25 */
//            /* 0x0068 */ unsigned long ReservedFlags5 : 2; /* bit position: 26 */
//            /* 0x0068 */ unsigned long Redirected : 1; /* bit position: 28 */
//            /* 0x0068 */ unsigned long ReservedFlags6 : 2; /* bit position: 29 */
//            /* 0x0068 */ unsigned long CompatDatabaseProcessed : 1; /* bit position: 31 */
//        }; /* bitfield */
//    }; /* size: 0x0004 */
//    /* 0x006c */ unsigned short ObsoleteLoadCount;
//    /* 0x006e */ unsigned short TlsIndex;
//    /* 0x0070 */ struct _LIST_ENTRY HashLinks;
//    /* 0x0080 */ unsigned long TimeDateStamp;
//    /* 0x0084 */ long Padding_770;
//    /* 0x0088 */ struct _ACTIVATION_CONTEXT* EntryPointActivationContext;
//    /* 0x0090 */ void* Lock;
//    /* 0x0098 */ struct _LDR_DDAG_NODE* DdagNode;
//    /* 0x00a0 */ struct _LIST_ENTRY NodeModuleLink;
//    /* 0x00b0 */ struct _LDRP_LOAD_CONTEXT* LoadContext;
//    /* 0x00b8 */ void* ParentDllBase;
//    /* 0x00c0 */ void* SwitchBackContext;
//    /* 0x00c8 */ struct _RTL_BALANCED_NODE BaseAddressIndexNode;
//    /* 0x00e0 */ struct _RTL_BALANCED_NODE MappingInfoIndexNode;
//    /* 0x00f8 */ unsigned __int64 OriginalBase;
//    /* 0x0100 */ union _LARGE_INTEGER LoadTime;
//    /* 0x0108 */ unsigned long BaseNameHashValue;
//    /* 0x010c */ enum _LDR_DLL_LOAD_REASON LoadReason;
//    /* 0x0110 */ unsigned long ImplicitPathOptions;
//    /* 0x0114 */ unsigned long ReferenceCount;
//    /* 0x0118 */ unsigned long DependentLoadFlags;
//    /* 0x011c */ unsigned char SigningLevel;
//    /* 0x011d */ char __PADDING__[3];
//} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY; /* size: 0x0120 */

typedef struct _TERMINATION_PORT
{
    /* 0x0000 */ struct _TERMINATION_PORT* Next;
    /* 0x0008 */ void* Port;
} TERMINATION_PORT, * PTERMINATION_PORT; /* size: 0x0010 */

typedef union _KALPC_DIRECT_EVENT
{
    union
    {
        /* 0x0000 */ unsigned __int64 Value;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 DirectType : 1; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 EventReferenced : 1; /* bit position: 1 */
            /* 0x0000 */ unsigned __int64 EventObjectBits : 62; /* bit position: 2 */
        }; /* bitfield */
    }; /* size: 0x0008 */
} KALPC_DIRECT_EVENT, * PKALPC_DIRECT_EVENT; /* size: 0x0008 */

typedef struct _ALPC_DISPATCH_CONTEXT
{
    /* 0x0000 */ struct _ALPC_PORT* PortObject;
    /* 0x0008 */ struct _KALPC_MESSAGE* Message;
    /* 0x0010 */ struct _ALPC_COMMUNICATION_INFO* CommunicationInfo;
    /* 0x0018 */ struct _ETHREAD* TargetThread;
    /* 0x0020 */ struct _ALPC_PORT* TargetPort;
    /* 0x0028 */ union _KALPC_DIRECT_EVENT DirectEvent;
    /* 0x0030 */ unsigned long Flags;
    /* 0x0034 */ unsigned short TotalLength;
    /* 0x0036 */ unsigned short Type;
    /* 0x0038 */ unsigned short DataInfoOffset;
    /* 0x003a */ unsigned char SignalCompletion;
    /* 0x003b */ unsigned char PostedToCompletionList;
    /* 0x003c */ long __PADDING__[1];
} ALPC_DISPATCH_CONTEXT, * PALPC_DISPATCH_CONTEXT; /* size: 0x0040 */

typedef struct _CLIENT_ID32
{
    /* 0x0000 */ unsigned long UniqueProcess;
    /* 0x0004 */ unsigned long UniqueThread;
} CLIENT_ID32, * PCLIENT_ID32; /* size: 0x0008 */

typedef struct _ACTIVATION_CONTEXT_STACK32
{
    /* 0x0000 */ unsigned long ActiveFrame;
    /* 0x0004 */ struct LIST_ENTRY32 FrameListCache;
    /* 0x000c */ unsigned long Flags;
    /* 0x0010 */ unsigned long NextCookieSequenceNumber;
    /* 0x0014 */ unsigned long StackId;
} ACTIVATION_CONTEXT_STACK32, * PACTIVATION_CONTEXT_STACK32; /* size: 0x0018 */

typedef struct _GDI_TEB_BATCH32
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned long Offset : 31; /* bit position: 0 */
        /* 0x0000 */ unsigned long HasRenderingCommand : 1; /* bit position: 31 */
    }; /* bitfield */
    /* 0x0004 */ unsigned long HDC;
    /* 0x0008 */ unsigned long Buffer[310];
} GDI_TEB_BATCH32, * PGDI_TEB_BATCH32; /* size: 0x04e0 */

typedef struct _TEB32
{
    /* 0x0000 */ struct _NT_TIB32 NtTib;
    /* 0x001c */ unsigned long EnvironmentPointer;
    /* 0x0020 */ struct _CLIENT_ID32 ClientId;
    /* 0x0028 */ unsigned long ActiveRpcHandle;
    /* 0x002c */ unsigned long ThreadLocalStoragePointer;
    /* 0x0030 */ unsigned long ProcessEnvironmentBlock;
    /* 0x0034 */ unsigned long LastErrorValue;
    /* 0x0038 */ unsigned long CountOfOwnedCriticalSections;
    /* 0x003c */ unsigned long CsrClientThread;
    /* 0x0040 */ unsigned long Win32ThreadInfo;
    /* 0x0044 */ unsigned long User32Reserved[26];
    /* 0x00ac */ unsigned long UserReserved[5];
    /* 0x00c0 */ unsigned long WOW32Reserved;
    /* 0x00c4 */ unsigned long CurrentLocale;
    /* 0x00c8 */ unsigned long FpSoftwareStatusRegister;
    /* 0x00cc */ unsigned long ReservedForDebuggerInstrumentation[16];
    /* 0x010c */ unsigned long SystemReserved1[26];
    /* 0x0174 */ char PlaceholderCompatibilityMode;
    /* 0x0175 */ unsigned char PlaceholderHydrationAlwaysExplicit;
    /* 0x0176 */ char PlaceholderReserved[10];
    /* 0x0180 */ unsigned long ProxiedProcessId;
    /* 0x0184 */ struct _ACTIVATION_CONTEXT_STACK32 _ActivationStack;
    /* 0x019c */ unsigned char WorkingOnBehalfTicket[8];
    /* 0x01a4 */ long ExceptionCode;
    /* 0x01a8 */ unsigned long ActivationContextStackPointer;
    /* 0x01ac */ unsigned long InstrumentationCallbackSp;
    /* 0x01b0 */ unsigned long InstrumentationCallbackPreviousPc;
    /* 0x01b4 */ unsigned long InstrumentationCallbackPreviousSp;
    /* 0x01b8 */ unsigned char InstrumentationCallbackDisabled;
    /* 0x01b9 */ unsigned char SpareBytes[23];
    /* 0x01d0 */ unsigned long TxFsContext;
    /* 0x01d4 */ struct _GDI_TEB_BATCH32 GdiTebBatch;
    /* 0x06b4 */ struct _CLIENT_ID32 RealClientId;
    /* 0x06bc */ unsigned long GdiCachedProcessHandle;
    /* 0x06c0 */ unsigned long GdiClientPID;
    /* 0x06c4 */ unsigned long GdiClientTID;
    /* 0x06c8 */ unsigned long GdiThreadLocalInfo;
    /* 0x06cc */ unsigned long Win32ClientInfo[62];
    /* 0x07c4 */ unsigned long glDispatchTable[233];
    /* 0x0b68 */ unsigned long glReserved1[29];
    /* 0x0bdc */ unsigned long glReserved2;
    /* 0x0be0 */ unsigned long glSectionInfo;
    /* 0x0be4 */ unsigned long glSection;
    /* 0x0be8 */ unsigned long glTable;
    /* 0x0bec */ unsigned long glCurrentRC;
    /* 0x0bf0 */ unsigned long glContext;
    /* 0x0bf4 */ unsigned long LastStatusValue;
    /* 0x0bf8 */ struct _STRING32 StaticUnicodeString;
    /* 0x0c00 */ wchar_t StaticUnicodeBuffer[261];
    /* 0x0e0a */ char Padding_553[2];
    /* 0x0e0c */ unsigned long DeallocationStack;
    /* 0x0e10 */ unsigned long TlsSlots[64];
    /* 0x0f10 */ struct LIST_ENTRY32 TlsLinks;
    /* 0x0f18 */ unsigned long Vdm;
    /* 0x0f1c */ unsigned long ReservedForNtRpc;
    /* 0x0f20 */ unsigned long DbgSsReserved[2];
    /* 0x0f28 */ unsigned long HardErrorMode;
    /* 0x0f2c */ unsigned long Instrumentation[9];
    /* 0x0f50 */ struct _GUID ActivityId;
    /* 0x0f60 */ unsigned long SubProcessTag;
    /* 0x0f64 */ unsigned long PerflibData;
    /* 0x0f68 */ unsigned long EtwTraceData;
    /* 0x0f6c */ unsigned long WinSockData;
    /* 0x0f70 */ unsigned long GdiBatchCount;
    union
    {
        /* 0x0f74 */ struct _PROCESSOR_NUMBER CurrentIdealProcessor;
        /* 0x0f74 */ unsigned long IdealProcessorValue;
        struct
        {
            /* 0x0f74 */ unsigned char ReservedPad0;
            /* 0x0f75 */ unsigned char ReservedPad1;
            /* 0x0f76 */ unsigned char ReservedPad2;
            /* 0x0f77 */ unsigned char IdealProcessor;
        }; /* size: 0x0004 */
    }; /* size: 0x0004 */
    /* 0x0f78 */ unsigned long GuaranteedStackBytes;
    /* 0x0f7c */ unsigned long ReservedForPerf;
    /* 0x0f80 */ unsigned long ReservedForOle;
    /* 0x0f84 */ unsigned long WaitingOnLoaderLock;
    /* 0x0f88 */ unsigned long SavedPriorityState;
    /* 0x0f8c */ unsigned long ReservedForCodeCoverage;
    /* 0x0f90 */ unsigned long ThreadPoolData;
    /* 0x0f94 */ unsigned long TlsExpansionSlots;
    /* 0x0f98 */ unsigned long MuiGeneration;
    /* 0x0f9c */ unsigned long IsImpersonating;
    /* 0x0fa0 */ unsigned long NlsCache;
    /* 0x0fa4 */ unsigned long pShimData;
    /* 0x0fa8 */ unsigned long HeapData;
    /* 0x0fac */ unsigned long CurrentTransactionHandle;
    /* 0x0fb0 */ unsigned long ActiveFrame;
    /* 0x0fb4 */ unsigned long FlsData;
    /* 0x0fb8 */ unsigned long PreferredLanguages;
    /* 0x0fbc */ unsigned long UserPrefLanguages;
    /* 0x0fc0 */ unsigned long MergedPrefLanguages;
    /* 0x0fc4 */ unsigned long MuiImpersonation;
    union
    {
        /* 0x0fc8 */ volatile unsigned short CrossTebFlags;
        /* 0x0fc8 */ unsigned short SpareCrossTebBits : 16; /* bit position: 0 */
    }; /* size: 0x0002 */
    union
    {
        /* 0x0fca */ unsigned short SameTebFlags;
        struct /* bitfield */
        {
            /* 0x0fca */ unsigned short SafeThunkCall : 1; /* bit position: 0 */
            /* 0x0fca */ unsigned short InDebugPrint : 1; /* bit position: 1 */
            /* 0x0fca */ unsigned short HasFiberData : 1; /* bit position: 2 */
            /* 0x0fca */ unsigned short SkipThreadAttach : 1; /* bit position: 3 */
            /* 0x0fca */ unsigned short WerInShipAssertCode : 1; /* bit position: 4 */
            /* 0x0fca */ unsigned short RanProcessInit : 1; /* bit position: 5 */
            /* 0x0fca */ unsigned short ClonedThread : 1; /* bit position: 6 */
            /* 0x0fca */ unsigned short SuppressDebugMsg : 1; /* bit position: 7 */
            /* 0x0fca */ unsigned short DisableUserStackWalk : 1; /* bit position: 8 */
            /* 0x0fca */ unsigned short RtlExceptionAttached : 1; /* bit position: 9 */
            /* 0x0fca */ unsigned short InitialThread : 1; /* bit position: 10 */
            /* 0x0fca */ unsigned short SessionAware : 1; /* bit position: 11 */
            /* 0x0fca */ unsigned short LoadOwner : 1; /* bit position: 12 */
            /* 0x0fca */ unsigned short LoaderWorker : 1; /* bit position: 13 */
            /* 0x0fca */ unsigned short SkipLoaderInit : 1; /* bit position: 14 */
            /* 0x0fca */ unsigned short SpareSameTebBits : 1; /* bit position: 15 */
        }; /* bitfield */
    }; /* size: 0x0002 */
    /* 0x0fcc */ unsigned long TxnScopeEnterCallback;
    /* 0x0fd0 */ unsigned long TxnScopeExitCallback;
    /* 0x0fd4 */ unsigned long TxnScopeContext;
    /* 0x0fd8 */ unsigned long LockCount;
    /* 0x0fdc */ long WowTebOffset;
    /* 0x0fe0 */ unsigned long ResourceRetValue;
    /* 0x0fe4 */ unsigned long ReservedForWdf;
    /* 0x0fe8 */ unsigned __int64 ReservedForCrt;
    /* 0x0ff0 */ struct _GUID EffectiveContainerId;
} TEB32, * PTEB32; /* size: 0x1000 */


typedef struct _MY_KAPC
{
    /* 0x0000 */ unsigned char Type;
    union
    {
        /* 0x0001 */ unsigned char AllFlags;
        struct /* bitfield */
        {
            /* 0x0001 */ unsigned char CallbackDataContext : 1; /* bit position: 0 */
            /* 0x0001 */ unsigned char Unused : 7; /* bit position: 1 */
        }; /* bitfield */
    }; /* size: 0x0001 */
    /* 0x0002 */ unsigned char Size;
    /* 0x0003 */ unsigned char SpareByte1;
    /* 0x0004 */ unsigned long SpareLong0;
    /* 0x0008 */ struct _KTHREAD* Thread;
    /* 0x0010 */ struct _LIST_ENTRY ApcListEntry;
    union
    {
        struct
        {
            /* 0x0020 */ void* KernelRoutine /* function */;
            /* 0x0028 */ PKRUNDOWN_ROUTINE RundownRoutine /* function */;
            /* 0x0030 */ void* NormalRoutine /* function */;
        }; /* size: 0x0018 */
        /* 0x0020 */ void* Reserved[3];
    }; /* size: 0x0018 */
    /* 0x0038 */ void* NormalContext;
    /* 0x0040 */ void* SystemArgument1;
    /* 0x0048 */ void* SystemArgument2;
    /* 0x0050 */ char ApcStateIndex;
    /* 0x0051 */ char ApcMode;
    /* 0x0052 */ unsigned char Inserted;
    /* 0x0053 */ char __PADDING__[5];
} MY_KAPC, * PMY_KAPC; /* size: 0x0058 */


typedef struct _ALPC_PORT_ATTRIBUTES
{
    /* 0x0000 */ unsigned long Flags;
    /* 0x0004 */ struct _SECURITY_QUALITY_OF_SERVICE SecurityQos;
    /* 0x0010 */ unsigned __int64 MaxMessageLength;
    /* 0x0018 */ unsigned __int64 MemoryBandwidth;
    /* 0x0020 */ unsigned __int64 MaxPoolUsage;
    /* 0x0028 */ unsigned __int64 MaxSectionSize;
    /* 0x0030 */ unsigned __int64 MaxViewSize;
    /* 0x0038 */ unsigned __int64 MaxTotalSectionSize;
    /* 0x0040 */ unsigned long DupObjectTypes;
    /* 0x0044 */ unsigned long Reserved;
} ALPC_PORT_ATTRIBUTES, * PALPC_PORT_ATTRIBUTES; /* size: 0x0048 */

typedef struct _ALPC_PORT
{
    /* 0x0000 */ struct _LIST_ENTRY PortListEntry;
    /* 0x0010 */ struct _ALPC_COMMUNICATION_INFO* CommunicationInfo;
    /* 0x0018 */ struct _EPROCESS* OwnerProcess;
    /* 0x0020 */ void* CompletionPort;
    /* 0x0028 */ void* CompletionKey;
    /* 0x0030 */ struct _ALPC_COMPLETION_PACKET_LOOKASIDE* CompletionPacketLookaside;
    /* 0x0038 */ void* PortContext;
    /* 0x0040 */ struct _SECURITY_CLIENT_CONTEXT StaticSecurity;
    /* 0x0088 */ struct _EX_PUSH_LOCK IncomingQueueLock;
    /* 0x0090 */ struct _LIST_ENTRY MainQueue;
    /* 0x00a0 */ struct _LIST_ENTRY LargeMessageQueue;
    /* 0x00b0 */ struct _EX_PUSH_LOCK PendingQueueLock;
    /* 0x00b8 */ struct _LIST_ENTRY PendingQueue;
    /* 0x00c8 */ struct _EX_PUSH_LOCK DirectQueueLock;
    /* 0x00d0 */ struct _LIST_ENTRY DirectQueue;
    /* 0x00e0 */ struct _EX_PUSH_LOCK WaitQueueLock;
    /* 0x00e8 */ struct _LIST_ENTRY WaitQueue;
    union
    {
        /* 0x00f8 */ struct _KSEMAPHORE* Semaphore;
        /* 0x00f8 */ struct _KEVENT* DummyEvent;
    }; /* size: 0x0008 */
    /* 0x0100 */ struct _ALPC_PORT_ATTRIBUTES PortAttributes;
    /* 0x0148 */ struct _EX_PUSH_LOCK ResourceListLock;
    /* 0x0150 */ struct _LIST_ENTRY ResourceListHead;
    /* 0x0160 */ struct _EX_PUSH_LOCK PortObjectLock;
    /* 0x0168 */ struct _ALPC_COMPLETION_LIST* CompletionList;
    /* 0x0170 */ struct _CALLBACK_OBJECT* CallbackObject;
    /* 0x0178 */ void* CallbackContext;
    /* 0x0180 */ struct _LIST_ENTRY CanceledQueue;
    /* 0x0190 */ long SequenceNo;
    /* 0x0194 */ long ReferenceNo;
    /* 0x0198 */ struct _PALPC_PORT_REFERENCE_WAIT_BLOCK* ReferenceNoWait;
    union
    {
        union
        {
            struct
            {
                struct /* bitfield */
                {
                    /* 0x01a0 */ unsigned long Initialized : 1; /* bit position: 0 */
                    /* 0x01a0 */ unsigned long Type : 2; /* bit position: 1 */
                    /* 0x01a0 */ unsigned long ConnectionPending : 1; /* bit position: 3 */
                    /* 0x01a0 */ unsigned long ConnectionRefused : 1; /* bit position: 4 */
                    /* 0x01a0 */ unsigned long Disconnected : 1; /* bit position: 5 */
                    /* 0x01a0 */ unsigned long Closed : 1; /* bit position: 6 */
                    /* 0x01a0 */ unsigned long NoFlushOnClose : 1; /* bit position: 7 */
                    /* 0x01a0 */ unsigned long ReturnExtendedInfo : 1; /* bit position: 8 */
                    /* 0x01a0 */ unsigned long Waitable : 1; /* bit position: 9 */
                    /* 0x01a0 */ unsigned long DynamicSecurity : 1; /* bit position: 10 */
                    /* 0x01a0 */ unsigned long Wow64CompletionList : 1; /* bit position: 11 */
                    /* 0x01a0 */ unsigned long Lpc : 1; /* bit position: 12 */
                    /* 0x01a0 */ unsigned long LpcToLpc : 1; /* bit position: 13 */
                    /* 0x01a0 */ unsigned long HasCompletionList : 1; /* bit position: 14 */
                    /* 0x01a0 */ unsigned long HadCompletionList : 1; /* bit position: 15 */
                    /* 0x01a0 */ unsigned long EnableCompletionList : 1; /* bit position: 16 */
                }; /* bitfield */
            } /* size: 0x0004 */ s1;
            /* 0x01a0 */ unsigned long State;
        }; /* size: 0x0004 */
    } /* size: 0x0004 */ u1;
    /* 0x01a4 */ long Padding_443;
    /* 0x01a8 */ struct _ALPC_PORT* TargetQueuePort;
    /* 0x01b0 */ struct _ALPC_PORT* TargetSequencePort;
    /* 0x01b8 */ struct _KALPC_MESSAGE* CachedMessage;
    /* 0x01c0 */ unsigned long MainQueueLength;
    /* 0x01c4 */ unsigned long LargeMessageQueueLength;
    /* 0x01c8 */ unsigned long PendingQueueLength;
    /* 0x01cc */ unsigned long DirectQueueLength;
    /* 0x01d0 */ unsigned long CanceledQueueLength;
    /* 0x01d4 */ unsigned long WaitQueueLength;
} ALPC_PORT, * PALPC_PORT; /* size: 0x01d8 */


typedef struct _MY_KEXCEPTION_FRAME
{
    /* 0x0000 */ unsigned __int64 P1Home;
    /* 0x0008 */ unsigned __int64 P2Home;
    /* 0x0010 */ unsigned __int64 P3Home;
    /* 0x0018 */ unsigned __int64 P4Home;
    /* 0x0020 */ unsigned __int64 P5;
    /* 0x0028 */ unsigned __int64 Spare1;
    /* 0x0030 */ struct _M128A Xmm6;
    /* 0x0040 */ struct _M128A Xmm7;
    /* 0x0050 */ struct _M128A Xmm8;
    /* 0x0060 */ struct _M128A Xmm9;
    /* 0x0070 */ struct _M128A Xmm10;
    /* 0x0080 */ struct _M128A Xmm11;
    /* 0x0090 */ struct _M128A Xmm12;
    /* 0x00a0 */ struct _M128A Xmm13;
    /* 0x00b0 */ struct _M128A Xmm14;
    /* 0x00c0 */ struct _M128A Xmm15;
    /* 0x00d0 */ unsigned __int64 TrapFrame;
    /* 0x00d8 */ unsigned __int64 OutputBuffer;
    /* 0x00e0 */ unsigned __int64 OutputLength;
    /* 0x00e8 */ unsigned __int64 Spare2;
    /* 0x00f0 */ unsigned __int64 MxCsr;
    /* 0x00f8 */ unsigned __int64 Rbp;
    /* 0x0100 */ unsigned __int64 Rbx;
    /* 0x0108 */ unsigned __int64 Rdi;
    /* 0x0110 */ unsigned __int64 Rsi;
    /* 0x0118 */ unsigned __int64 R12;
    /* 0x0120 */ unsigned __int64 R13;
    /* 0x0128 */ unsigned __int64 R14;
    /* 0x0130 */ unsigned __int64 R15;
    /* 0x0138 */ unsigned __int64 Return;
} MY_KEXCEPTION_FRAME, * PMY_KEXCEPTION_FRAME; /* size: 0x0140 */


typedef struct _MY_KTRAP_FRAME
{
    /* 0x0000 */ unsigned __int64 P1Home;
    /* 0x0008 */ unsigned __int64 P2Home;
    /* 0x0010 */ unsigned __int64 P3Home;
    /* 0x0018 */ unsigned __int64 P4Home;
    /* 0x0020 */ unsigned __int64 P5;
    union
    {
        /* 0x0028 */ char PreviousMode;
        /* 0x0028 */ unsigned char InterruptRetpolineState;
    }; /* size: 0x0001 */
    /* 0x0029 */ unsigned char PreviousIrql;
    union
    {
        /* 0x002a */ unsigned char FaultIndicator;
        /* 0x002a */ unsigned char NmiMsrIbrs;
    }; /* size: 0x0001 */
    /* 0x002b */ unsigned char ExceptionActive;
    /* 0x002c */ unsigned long MxCsr;
    /* 0x0030 */ unsigned __int64 Rax;
    /* 0x0038 */ unsigned __int64 Rcx;
    /* 0x0040 */ unsigned __int64 Rdx;
    /* 0x0048 */ unsigned __int64 R8;
    /* 0x0050 */ unsigned __int64 R9;
    /* 0x0058 */ unsigned __int64 R10;
    /* 0x0060 */ unsigned __int64 R11;
    union
    {
        /* 0x0068 */ unsigned __int64 GsBase;
        /* 0x0068 */ unsigned __int64 GsSwap;
    }; /* size: 0x0008 */
    /* 0x0070 */ struct _M128A Xmm0;
    /* 0x0080 */ struct _M128A Xmm1;
    /* 0x0090 */ struct _M128A Xmm2;
    /* 0x00a0 */ struct _M128A Xmm3;
    /* 0x00b0 */ struct _M128A Xmm4;
    /* 0x00c0 */ struct _M128A Xmm5;
    union
    {
        /* 0x00d0 */ unsigned __int64 FaultAddress;
        /* 0x00d0 */ unsigned __int64 ContextRecord;
    }; /* size: 0x0008 */
    /* 0x00d8 */ unsigned __int64 Dr0;
    /* 0x00e0 */ unsigned __int64 Dr1;
    /* 0x00e8 */ unsigned __int64 Dr2;
    /* 0x00f0 */ unsigned __int64 Dr3;
    /* 0x00f8 */ unsigned __int64 Dr6;
    /* 0x0100 */ unsigned __int64 Dr7;
    /* 0x0108 */ unsigned __int64 DebugControl;
    /* 0x0110 */ unsigned __int64 LastBranchToRip;
    /* 0x0118 */ unsigned __int64 LastBranchFromRip;
    /* 0x0120 */ unsigned __int64 LastExceptionToRip;
    /* 0x0128 */ unsigned __int64 LastExceptionFromRip;
    /* 0x0130 */ unsigned short SegDs;
    /* 0x0132 */ unsigned short SegEs;
    /* 0x0134 */ unsigned short SegFs;
    /* 0x0136 */ unsigned short SegGs;
    /* 0x0138 */ unsigned __int64 TrapFrame;
    union
    {
        struct
        {
            /* 0x0140 */ unsigned long NmiPreviousSpecCtrl;
            /* 0x0144 */ unsigned long NmiPreviousSpecCtrlPad;
        }; /* size: 0x0008 */
        /* 0x0140 */ unsigned __int64 Rbx;
    }; /* size: 0x0008 */
    /* 0x0148 */ unsigned __int64 Rdi;
    /* 0x0150 */ unsigned __int64 Rsi;
    /* 0x0158 */ unsigned __int64 Rbp;
    union
    {
        /* 0x0160 */ unsigned __int64 ErrorCode;
        /* 0x0160 */ unsigned __int64 ExceptionFrame;
    }; /* size: 0x0008 */
    /* 0x0168 */ unsigned __int64 Rip;
    /* 0x0170 */ unsigned short SegCs;
    /* 0x0172 */ unsigned char Fill0;
    /* 0x0173 */ unsigned char Logging;
    /* 0x0174 */ unsigned short Fill1[2];
    /* 0x0178 */ unsigned long EFlags;
    /* 0x017c */ unsigned long Fill2;
    /* 0x0180 */ unsigned __int64 Rsp;
    /* 0x0188 */ unsigned short SegSs;
    /* 0x018a */ unsigned short Fill3;
    /* 0x018c */ unsigned long Fill4;
} MY_KTRAP_FRAME, * PMY_KTRAP_FRAME; /* size: 0x0190 */

typedef struct _MY_XSAVE_AREA_HEADER
{
    /* 0x0000 */ unsigned __int64 Mask;
    /* 0x0008 */ unsigned __int64 CompactionMask;
    /* 0x0010 */ unsigned __int64 Reserved2[6];
} MY_XSAVE_AREA_HEADER, * PMY_XSAVE_AREA_HEADER; /* size: 0x0040 */

typedef union _JOBOBJECT_ENERGY_TRACKING_STATE
{
    union
    {
        /* 0x0000 */ unsigned __int64 Value;
        struct
        {
            /* 0x0000 */ unsigned long UpdateMask;
            /* 0x0004 */ unsigned long DesiredState;
        }; /* size: 0x0008 */
    }; /* size: 0x0008 */
} JOBOBJECT_ENERGY_TRACKING_STATE, * PJOBOBJECT_ENERGY_TRACKING_STATE; /* size: 0x0008 */

typedef struct _PROCESS_DISK_COUNTERS
{
    /* 0x0000 */ unsigned __int64 BytesRead;
    /* 0x0008 */ unsigned __int64 BytesWritten;
    /* 0x0010 */ unsigned __int64 ReadOperationCount;
    /* 0x0018 */ unsigned __int64 WriteOperationCount;
    /* 0x0020 */ unsigned __int64 FlushOperationCount;
} PROCESS_DISK_COUNTERS, * PPROCESS_DISK_COUNTERS; /* size: 0x0028 */

typedef struct _PS_JOB_WAKE_INFORMATION
{
    /* 0x0000 */ unsigned __int64 NotificationChannel;
    /* 0x0008 */ unsigned __int64 WakeCounters[7];
    /* 0x0040 */ unsigned __int64 NoWakeCounter;
} PS_JOB_WAKE_INFORMATION, * PPS_JOB_WAKE_INFORMATION; /* size: 0x0048 */

typedef struct _EPROCESS_VALUES
{
    /* 0x0000 */ unsigned __int64 KernelTime;
    /* 0x0008 */ unsigned __int64 UserTime;
    /* 0x0010 */ unsigned __int64 ReadyTime;
    /* 0x0018 */ unsigned __int64 CycleTime;
    /* 0x0020 */ unsigned __int64 ContextSwitches;
    /* 0x0028 */ __int64 ReadOperationCount;
    /* 0x0030 */ __int64 WriteOperationCount;
    /* 0x0038 */ __int64 OtherOperationCount;
    /* 0x0040 */ __int64 ReadTransferCount;
    /* 0x0048 */ __int64 WriteTransferCount;
    /* 0x0050 */ __int64 OtherTransferCount;
    /* 0x0058 */ unsigned __int64 KernelWaitTime;
    /* 0x0060 */ unsigned __int64 UserWaitTime;
} EPROCESS_VALUES, * PEPROCESS_VALUES; /* size: 0x0068 */

typedef struct _PS_IO_CONTROL_ENTRY
{
    union
    {
        /* 0x0000 */ struct _RTL_BALANCED_NODE VolumeTreeNode;
        struct
        {
            /* 0x0000 */ struct _LIST_ENTRY FreeListEntry;
            /* 0x0010 */ unsigned __int64 ReservedForParentValue;
        }; /* size: 0x0018 */
    }; /* size: 0x0018 */
    /* 0x0018 */ unsigned __int64 VolumeKey;
    /* 0x0020 */ struct _EX_RUNDOWN_REF Rundown;
    /* 0x0028 */ void* IoControl;
    /* 0x0030 */ void* VolumeIoAttribution;
} PS_IO_CONTROL_ENTRY, * PPS_IO_CONTROL_ENTRY; /* size: 0x0038 */

typedef struct _JOB_RATE_CONTROL_HEADER
{
    /* 0x0000 */ void* RateControlQuotaReference;
    /* 0x0008 */ struct _RTL_BITMAP OverQuotaHistory;
    /* 0x0018 */ unsigned char* BitMapBuffer;
    /* 0x0020 */ unsigned __int64 BitMapBufferSize;
} JOB_RATE_CONTROL_HEADER, * PJOB_RATE_CONTROL_HEADER; /* size: 0x0028 */

typedef struct _EJOB
{
    /* 0x0000 */ struct _KEVENT Event;
    /* 0x0018 */ struct _LIST_ENTRY JobLinks;
    /* 0x0028 */ struct _LIST_ENTRY ProcessListHead;
    /* 0x0038 */ struct _ERESOURCE JobLock;
    /* 0x00a0 */ union _LARGE_INTEGER TotalUserTime;
    /* 0x00a8 */ union _LARGE_INTEGER TotalKernelTime;
    /* 0x00b0 */ union _LARGE_INTEGER TotalCycleTime;
    /* 0x00b8 */ union _LARGE_INTEGER ThisPeriodTotalUserTime;
    /* 0x00c0 */ union _LARGE_INTEGER ThisPeriodTotalKernelTime;
    /* 0x00c8 */ unsigned __int64 TotalContextSwitches;
    /* 0x00d0 */ unsigned long TotalPageFaultCount;
    /* 0x00d4 */ unsigned long TotalProcesses;
    /* 0x00d8 */ unsigned long ActiveProcesses;
    /* 0x00dc */ unsigned long TotalTerminatedProcesses;
    /* 0x00e0 */ union _LARGE_INTEGER PerProcessUserTimeLimit;
    /* 0x00e8 */ union _LARGE_INTEGER PerJobUserTimeLimit;
    /* 0x00f0 */ unsigned __int64 MinimumWorkingSetSize;
    /* 0x00f8 */ unsigned __int64 MaximumWorkingSetSize;
    /* 0x0100 */ unsigned long LimitFlags;
    /* 0x0104 */ unsigned long ActiveProcessLimit;
    /* 0x0108 */ struct _KAFFINITY_EX Affinity;
    /* 0x01b0 */ struct _JOB_ACCESS_STATE* AccessState;
    /* 0x01b8 */ void* AccessStateQuotaReference;
    /* 0x01c0 */ unsigned long UIRestrictionsClass;
    /* 0x01c4 */ unsigned long EndOfJobTimeAction;
    /* 0x01c8 */ void* CompletionPort;
    /* 0x01d0 */ void* CompletionKey;
    /* 0x01d8 */ unsigned __int64 CompletionCount;
    /* 0x01e0 */ unsigned long SessionId;
    /* 0x01e4 */ unsigned long SchedulingClass;
    /* 0x01e8 */ unsigned __int64 ReadOperationCount;
    /* 0x01f0 */ unsigned __int64 WriteOperationCount;
    /* 0x01f8 */ unsigned __int64 OtherOperationCount;
    /* 0x0200 */ unsigned __int64 ReadTransferCount;
    /* 0x0208 */ unsigned __int64 WriteTransferCount;
    /* 0x0210 */ unsigned __int64 OtherTransferCount;
    /* 0x0218 */ struct _PROCESS_DISK_COUNTERS DiskIoInfo;
    /* 0x0240 */ unsigned __int64 ProcessMemoryLimit;
    /* 0x0248 */ unsigned __int64 JobMemoryLimit;
    /* 0x0250 */ unsigned __int64 JobTotalMemoryLimit;
    /* 0x0258 */ unsigned __int64 PeakProcessMemoryUsed;
    /* 0x0260 */ unsigned __int64 PeakJobMemoryUsed;
    /* 0x0268 */ struct _KAFFINITY_EX EffectiveAffinity;
    /* 0x0310 */ union _LARGE_INTEGER EffectivePerProcessUserTimeLimit;
    /* 0x0318 */ unsigned __int64 EffectiveMinimumWorkingSetSize;
    /* 0x0320 */ unsigned __int64 EffectiveMaximumWorkingSetSize;
    /* 0x0328 */ unsigned __int64 EffectiveProcessMemoryLimit;
    /* 0x0330 */ struct _EJOB* EffectiveProcessMemoryLimitJob;
    /* 0x0338 */ struct _EJOB* EffectivePerProcessUserTimeLimitJob;
    /* 0x0340 */ struct _EJOB* EffectiveNetIoRateLimitJob;
    /* 0x0348 */ struct _EJOB* EffectiveHeapAttributionJob;
    /* 0x0350 */ unsigned long EffectiveLimitFlags;
    /* 0x0354 */ unsigned long EffectiveSchedulingClass;
    /* 0x0358 */ unsigned long EffectiveFreezeCount;
    /* 0x035c */ unsigned long EffectiveBackgroundCount;
    /* 0x0360 */ unsigned long EffectiveSwapCount;
    /* 0x0364 */ unsigned long EffectiveNotificationLimitCount;
    /* 0x0368 */ unsigned char EffectivePriorityClass;
    /* 0x0369 */ unsigned char PriorityClass;
    /* 0x036a */ unsigned char NestingDepth;
    /* 0x036b */ unsigned char Reserved1[1];
    /* 0x036c */ unsigned long CompletionFilter;
    union
    {
        /* 0x0370 */ struct _WNF_STATE_NAME WakeChannel;
        /* 0x0370 */ struct _PS_JOB_WAKE_INFORMATION WakeInfo;
    }; /* size: 0x0048 */
    /* 0x03b8 */ struct _JOBOBJECT_WAKE_FILTER WakeFilter;
    /* 0x03c0 */ unsigned long LowEdgeLatchFilter;
    /* 0x03c4 */ long Padding_325;
    /* 0x03c8 */ struct _EJOB* NotificationLink;
    /* 0x03d0 */ unsigned __int64 CurrentJobMemoryUsed;
    /* 0x03d8 */ struct _JOB_NOTIFICATION_INFORMATION* NotificationInfo;
    /* 0x03e0 */ void* NotificationInfoQuotaReference;
    /* 0x03e8 */ struct _IO_MINI_COMPLETION_PACKET_USER* NotificationPacket;
    /* 0x03f0 */ struct _JOB_CPU_RATE_CONTROL* CpuRateControl;
    /* 0x03f8 */ void* EffectiveSchedulingGroup;
    /* 0x0400 */ unsigned __int64 ReadyTime;
    /* 0x0408 */ struct _EX_PUSH_LOCK MemoryLimitsLock;
    /* 0x0410 */ struct _LIST_ENTRY SiblingJobLinks;
    /* 0x0420 */ struct _LIST_ENTRY ChildJobListHead;
    /* 0x0430 */ struct _EJOB* ParentJob;
    /* 0x0438 */ struct _EJOB* volatile RootJob;
    /* 0x0440 */ struct _LIST_ENTRY IteratorListHead;
    /* 0x0450 */ unsigned __int64 AncestorCount;
    union
    {
        /* 0x0458 */ struct _EJOB** Ancestors;
        /* 0x0458 */ void* SessionObject;
    }; /* size: 0x0008 */
    /* 0x0460 */ struct _EPROCESS_VALUES Accounting;
    /* 0x04c8 */ unsigned long ShadowActiveProcessCount;
    /* 0x04cc */ unsigned long ActiveAuxiliaryProcessCount;
    /* 0x04d0 */ unsigned long SequenceNumber;
    /* 0x04d4 */ unsigned long JobId;
    /* 0x04d8 */ struct _GUID ContainerId;
    /* 0x04e8 */ struct _GUID ContainerTelemetryId;
    /* 0x04f8 */ struct _ESERVERSILO_GLOBALS* ServerSiloGlobals;
    /* 0x0500 */ struct _PS_PROPERTY_SET PropertySet;
    /* 0x0518 */ struct _PSP_STORAGE* Storage;
    /* 0x0520 */ struct _JOB_NET_RATE_CONTROL* NetRateControl;
    union
    {
        /* 0x0528 */ unsigned long JobFlags;
        struct /* bitfield */
        {
            /* 0x0528 */ unsigned long CloseDone : 1; /* bit position: 0 */
            /* 0x0528 */ unsigned long MultiGroup : 1; /* bit position: 1 */
            /* 0x0528 */ unsigned long OutstandingNotification : 1; /* bit position: 2 */
            /* 0x0528 */ unsigned long NotificationInProgress : 1; /* bit position: 3 */
            /* 0x0528 */ unsigned long UILimits : 1; /* bit position: 4 */
            /* 0x0528 */ unsigned long CpuRateControlActive : 1; /* bit position: 5 */
            /* 0x0528 */ unsigned long OwnCpuRateControl : 1; /* bit position: 6 */
            /* 0x0528 */ unsigned long Terminating : 1; /* bit position: 7 */
            /* 0x0528 */ unsigned long WorkingSetLock : 1; /* bit position: 8 */
            /* 0x0528 */ unsigned long JobFrozen : 1; /* bit position: 9 */
            /* 0x0528 */ unsigned long Background : 1; /* bit position: 10 */
            /* 0x0528 */ unsigned long WakeNotificationAllocated : 1; /* bit position: 11 */
            /* 0x0528 */ unsigned long WakeNotificationEnabled : 1; /* bit position: 12 */
            /* 0x0528 */ unsigned long WakeNotificationPending : 1; /* bit position: 13 */
            /* 0x0528 */ unsigned long LimitNotificationRequired : 1; /* bit position: 14 */
            /* 0x0528 */ unsigned long ZeroCountNotificationRequired : 1; /* bit position: 15 */
            /* 0x0528 */ unsigned long CycleTimeNotificationRequired : 1; /* bit position: 16 */
            /* 0x0528 */ unsigned long CycleTimeNotificationPending : 1; /* bit position: 17 */
            /* 0x0528 */ unsigned long TimersVirtualized : 1; /* bit position: 18 */
            /* 0x0528 */ unsigned long JobSwapped : 1; /* bit position: 19 */
            /* 0x0528 */ unsigned long ViolationDetected : 1; /* bit position: 20 */
            /* 0x0528 */ unsigned long EmptyJobNotified : 1; /* bit position: 21 */
            /* 0x0528 */ unsigned long NoSystemCharge : 1; /* bit position: 22 */
            /* 0x0528 */ unsigned long DropNoWakeCharges : 1; /* bit position: 23 */
            /* 0x0528 */ unsigned long NoWakeChargePolicyDecided : 1; /* bit position: 24 */
            /* 0x0528 */ unsigned long NetRateControlActive : 1; /* bit position: 25 */
            /* 0x0528 */ unsigned long OwnNetRateControl : 1; /* bit position: 26 */
            /* 0x0528 */ unsigned long IoRateControlActive : 1; /* bit position: 27 */
            /* 0x0528 */ unsigned long OwnIoRateControl : 1; /* bit position: 28 */
            /* 0x0528 */ unsigned long DisallowNewProcesses : 1; /* bit position: 29 */
            /* 0x0528 */ unsigned long Silo : 1; /* bit position: 30 */
            /* 0x0528 */ unsigned long ContainerTelemetryIdSet : 1; /* bit position: 31 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    union
    {
        /* 0x052c */ unsigned long JobFlags2;
        struct /* bitfield */
        {
            /* 0x052c */ unsigned long ParentLocked : 1; /* bit position: 0 */
            /* 0x052c */ unsigned long EnableUsermodeSiloThreadImpersonation : 1; /* bit position: 1 */
            /* 0x052c */ unsigned long DisallowUsermodeSiloThreadImpersonation : 1; /* bit position: 2 */
        }; /* bitfield */
    }; /* size: 0x0004 */
    /* 0x0530 */ struct _PROCESS_EXTENDED_ENERGY_VALUES* EnergyValues;
    /* 0x0538 */ volatile unsigned __int64 SharedCommitCharge;
    /* 0x0540 */ unsigned long DiskIoAttributionUserRefCount;
    /* 0x0544 */ unsigned long DiskIoAttributionRefCount;
    union
    {
        /* 0x0548 */ void* DiskIoAttributionContext;
        /* 0x0548 */ struct _EJOB* DiskIoAttributionOwnerJob;
    }; /* size: 0x0008 */
    /* 0x0550 */ struct _JOB_RATE_CONTROL_HEADER IoRateControlHeader;
    /* 0x0578 */ struct _PS_IO_CONTROL_ENTRY GlobalIoControl;
    /* 0x05b0 */ volatile long IoControlStateLock;
    /* 0x05b4 */ long Padding_326;
    /* 0x05b8 */ struct _RTL_RB_TREE VolumeIoControlTree;
    /* 0x05c8 */ unsigned __int64 IoRateOverQuotaHistory;
    /* 0x05d0 */ unsigned long IoRateCurrentGeneration;
    /* 0x05d4 */ unsigned long IoRateLastQueryGeneration;
    /* 0x05d8 */ unsigned long IoRateGenerationLength;
    /* 0x05dc */ unsigned long IoRateOverQuotaNotifySequenceId;
    /* 0x05e0 */ unsigned __int64 LastThrottledIoTime;
    /* 0x05e8 */ struct _EX_PUSH_LOCK IoControlLock;
    /* 0x05f0 */ __int64 SiloHardReferenceCount;
    /* 0x05f8 */ struct _WORK_QUEUE_ITEM RundownWorkItem;
    /* 0x0618 */ void* PartitionObject;
    /* 0x0620 */ struct _EJOB* PartitionOwnerJob;
    /* 0x0628 */ union _JOBOBJECT_ENERGY_TRACKING_STATE EnergyTrackingState;
    /* 0x0630 */ unsigned __int64 KernelWaitTime;
    /* 0x0638 */ unsigned __int64 UserWaitTime;
} EJOB, * PEJOB; /* size: 0x0640 */

typedef struct _INITIAL_TEB
{
    struct
    {
        /* 0x0000 */ void* OldStackBase;
        /* 0x0008 */ void* OldStackLimit;
    } /* size: 0x0010 */ OldInitialTeb;
    /* 0x0010 */ void* StackBase;
    /* 0x0018 */ void* StackLimit;
    /* 0x0020 */ void* StackAllocationBase;
} INITIAL_TEB, * PINITIAL_TEB; /* size: 0x0028 */


typedef struct _PS_MEMORY_RESERVE
{
    /* 0x0000 */ void* ReserveAddress;
    /* 0x0008 */ unsigned __int64 ReserveSize;
} PS_MEMORY_RESERVE, * PPS_MEMORY_RESERVE; /* size: 0x0010 */


typedef struct _UMS_CREATE_THREAD_ATTRIBUTES
{
    /* 0x0000 */ unsigned long UmsVersion;
    /* 0x0004 */ long Padding_511;
    union
    {
        /* 0x0008 */ struct _RTL_UMS_CONTEXT* RtlUmsContext;
        /* 0x0008 */ void* UmsContext;
    }; /* size: 0x0008 */
    union
    {
        /* 0x0010 */ struct _RTL_UMS_COMPLETION_LIST* RtlUmsCompletionList;
        /* 0x0010 */ void* UmsCompletionList;
    }; /* size: 0x0008 */
} UMS_CREATE_THREAD_ATTRIBUTES, * PUMS_CREATE_THREAD_ATTRIBUTES; /* size: 0x0018 */

typedef struct _PSP_CREATE_PROCESS_CONTEXT
{
    /* 0x0000 */ unsigned long PresentFlags;
    union
    {
        /* 0x0004 */ unsigned long StateFlags;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0004 */ unsigned char ThreadContext : 1; /* bit position: 0 */
                /* 0x0004 */ unsigned char WriteOutputOnExit : 1; /* bit position: 1 */
                /* 0x0004 */ unsigned char FreeProcessParameters : 1; /* bit position: 2 */
                /* 0x0004 */ unsigned char IFEOKeyMissing : 1; /* bit position: 3 */
                /* 0x0004 */ unsigned char SanitizeProcessParameters : 1; /* bit position: 4 */
                /* 0x0004 */ unsigned char DetectManifest : 1; /* bit position: 5 */
                /* 0x0004 */ unsigned char ManifestDetected : 1; /* bit position: 6 */
                /* 0x0004 */ unsigned char SpareBool : 1; /* bit position: 7 */
            }; /* bitfield */
            struct /* bitfield */
            {
                /* 0x0005 */ unsigned char IFEOKeyState : 2; /* bit position: 0 */
                /* 0x0005 */ unsigned char StdHandleState : 2; /* bit position: 2 */
                /* 0x0005 */ unsigned char PseudoHandleMask : 3; /* bit position: 4 */
                /* 0x0005 */ unsigned char SpareBits : 1; /* bit position: 7 */
            }; /* bitfield */
            /* 0x0006 */ unsigned short ProhibitedImageCharacteristics : 16; /* bit position: 0 */
        }; /* size: 0x0004 */
    }; /* size: 0x0004 */
    /* 0x0008 */ struct _CLIENT_ID* RetClientId;
    /* 0x0010 */ struct _TEB** RetTeb;
    /* 0x0018 */ struct _SECTION_IMAGE_INFORMATION* RetImageInfo;
    /* 0x0020 */ struct _PS_CREATE_INFO* RetCreateInfo;
    /* 0x0028 */ struct _SECTION_IMAGE_INFORMATION SectionInfo;
    /* 0x0068 */ void* ParentHandle;
    /* 0x0070 */ struct _EPROCESS* ParentProcess;
    /* 0x0078 */ void* DebugHandle;
    /* 0x0080 */ void* TokenHandle;
    /* 0x0088 */ unsigned long AdditionalFileAccess;
    /* 0x008c */ long Padding_1097;
    /* 0x0090 */ void* FileHandle;
    /* 0x0098 */ struct _FILE_OBJECT* FileObject;
    /* 0x00a0 */ void* SectionHandle;
    /* 0x00a8 */ void* IFEOKey;
    /* 0x00b0 */ void* SectionObject;
    /* 0x00b8 */ struct _RTL_USER_PROCESS_PARAMETERS* CapturedProcessParameters;
    /* 0x00c0 */ struct _RTL_USER_PROCESS_PARAMETERS* UserProcessParameters;
    /* 0x00c8 */ unsigned long UserProcessParametersWow64;
    /* 0x00cc */ unsigned long PebWow64;
    /* 0x00d0 */ struct _UNICODE_STRING FileName;
    /* 0x00e0 */ unsigned char PriorityClass;
    /* 0x00e1 */ unsigned char ExecuteOptions;
    /* 0x00e2 */ unsigned short NodeNumber;
    /* 0x00e4 */ unsigned long IdealProcessor;
    /* 0x00e8 */ unsigned __int64 ReserveEntryCount;
    /* 0x00f0 */ struct _PS_MEMORY_RESERVE LocalReserveList[1];
    /* 0x0100 */ struct _PS_MEMORY_RESERVE* CapturedReserveList;
    /* 0x0108 */ unsigned long StdHandleSubsystemType;
    /* 0x010c */ unsigned long HandleCount;
    /* 0x0110 */ void** HandleList;
    /* 0x0118 */ void* ManifestAddress;
    /* 0x0120 */ unsigned long ManifestSize;
    /* 0x0124 */ unsigned long ErrorMode;
    /* 0x0128 */ struct _GROUP_AFFINITY GroupAffinity;
    /* 0x0138 */ struct _UMS_CREATE_THREAD_ATTRIBUTES UmsAttributes;
} PSP_CREATE_PROCESS_CONTEXT, * PPSP_CREATE_PROCESS_CONTEXT; /* size: 0x0150 */

typedef union _PSP_CPU_QUOTA_APC
{
    union
    {
        /* 0x0000 */ struct _KAPC Apc;
        struct
        {
            /* 0x0000 */ unsigned char ApcFill[83];
            /* 0x0053 */ unsigned char ApcPending;
        }; /* size: 0x0054 */
        /* 0x0000 */ union _PSP_CPU_QUOTA_APC* Next;
    }; /* size: 0x0058 */
} PSP_CPU_QUOTA_APC, * PPSP_CPU_QUOTA_APC; /* size: 0x0058 */


typedef struct _RTL_DRIVE_LETTER_CURDIR
{
    /* 0x0000 */ unsigned short Flags;
    /* 0x0002 */ unsigned short Length;
    /* 0x0004 */ unsigned long TimeStamp;
    /* 0x0008 */ struct _STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, * PRTL_DRIVE_LETTER_CURDIR; /* size: 0x0018 */

typedef struct _CURDIR
{
    /* 0x0000 */ struct _UNICODE_STRING DosPath;
    /* 0x0010 */ void* Handle;
} CURDIR, * PCURDIR; /* size: 0x0018 */

typedef struct _RTL_USER_PROCESS_PARAMETERS
{
    /* 0x0000 */ unsigned long MaximumLength;
    /* 0x0004 */ unsigned long Length;
    /* 0x0008 */ unsigned long Flags;
    /* 0x000c */ unsigned long DebugFlags;
    /* 0x0010 */ void* ConsoleHandle;
    /* 0x0018 */ unsigned long ConsoleFlags;
    /* 0x001c */ long Padding_164;
    /* 0x0020 */ void* StandardInput;
    /* 0x0028 */ void* StandardOutput;
    /* 0x0030 */ void* StandardError;
    /* 0x0038 */ struct _CURDIR CurrentDirectory;
    /* 0x0050 */ struct _UNICODE_STRING DllPath;
    /* 0x0060 */ struct _UNICODE_STRING ImagePathName;
    /* 0x0070 */ struct _UNICODE_STRING CommandLine;
    /* 0x0080 */ void* Environment;
    /* 0x0088 */ unsigned long StartingX;
    /* 0x008c */ unsigned long StartingY;
    /* 0x0090 */ unsigned long CountX;
    /* 0x0094 */ unsigned long CountY;
    /* 0x0098 */ unsigned long CountCharsX;
    /* 0x009c */ unsigned long CountCharsY;
    /* 0x00a0 */ unsigned long FillAttribute;
    /* 0x00a4 */ unsigned long WindowFlags;
    /* 0x00a8 */ unsigned long ShowWindowFlags;
    /* 0x00ac */ long Padding_165;
    /* 0x00b0 */ struct _UNICODE_STRING WindowTitle;
    /* 0x00c0 */ struct _UNICODE_STRING DesktopInfo;
    /* 0x00d0 */ struct _UNICODE_STRING ShellInfo;
    /* 0x00e0 */ struct _UNICODE_STRING RuntimeData;
    /* 0x00f0 */ struct _RTL_DRIVE_LETTER_CURDIR CurrentDirectores[32];
    /* 0x03f0 */ unsigned __int64 EnvironmentSize;
    /* 0x03f8 */ unsigned __int64 EnvironmentVersion;
    /* 0x0400 */ void* PackageDependencyData;
    /* 0x0408 */ unsigned long ProcessGroupId;
    /* 0x040c */ unsigned long LoaderThreads;
    /* 0x0410 */ struct _UNICODE_STRING RedirectionDllName;
    /* 0x0420 */ struct _UNICODE_STRING HeapPartitionName;
    /* 0x0430 */ unsigned __int64* DefaultThreadpoolCpuSetMasks;
    /* 0x0438 */ unsigned long DefaultThreadpoolCpuSetMaskCount;
    /* 0x043c */ unsigned long DefaultThreadpoolThreadMaximum;
} RTL_USER_PROCESS_PARAMETERS, * PRTL_USER_PROCESS_PARAMETERS; /* size: 0x0440 */


typedef struct _PS_INITIAL_TEB
{
    struct
    {
        /* 0x0000 */ void* OldStackBase;
        /* 0x0008 */ void* OldStackLimit;
    } /* size: 0x0010 */ OldInitialTeb;
    /* 0x0010 */ void* StackBase;
    /* 0x0018 */ void* StackLimit;
    /* 0x0020 */ void* StackAllocationBase;
    /* 0x0028 */ void* Wow64StackBase;
    /* 0x0030 */ void* Wow64StackLimit;
    /* 0x0038 */ void* Wow64StackAllocationBase;
} PS_INITIAL_TEB, * PPS_INITIAL_TEB; /* size: 0x0040 */

typedef struct _PS_ATTRIBUTE
{
    /* 0x0000 */ unsigned __int64 Attribute;
    /* 0x0008 */ unsigned __int64 Size;
    union
    {
        /* 0x0010 */ unsigned __int64 Value;
        /* 0x0010 */ void* ValuePtr;
    }; /* size: 0x0008 */
    /* 0x0018 */ unsigned __int64* ReturnLength;
} PS_ATTRIBUTE, * PPS_ATTRIBUTE; /* size: 0x0020 */

typedef struct _PS_ATTRIBUTE_LIST
{
    /* 0x0000 */ unsigned __int64 TotalLength;
    /* 0x0008 */ struct _PS_ATTRIBUTE Attributes[1];
} PS_ATTRIBUTE_LIST, * PPS_ATTRIBUTE_LIST; /* size: 0x0028 */

typedef struct _PSP_QUOTA_ENTRY
{
    union
    {
        /* 0x0000 */ unsigned char CacheAlign[64];
        struct
        {
            /* 0x0000 */ unsigned __int64 Usage;
            /* 0x0008 */ unsigned __int64 Peak;
        }; /* size: 0x0010 */
    }; /* size: 0x0040 */
    /* 0x0040 */ unsigned __int64 Limit;
    /* 0x0048 */ unsigned __int64 Return;
    /* 0x0050 */ struct _LIST_ENTRY ExpansionLink;
    /* 0x0060 */ long __PADDING__[8];
} PSP_QUOTA_ENTRY, * PPSP_QUOTA_ENTRY; /* size: 0x0080 */

typedef struct _EPROCESS_QUOTA_BLOCK
{
    /* 0x0000 */ struct _PSP_QUOTA_ENTRY QuotaEntry[4];
    /* 0x0200 */ struct _PS_CPU_QUOTA_BLOCK* CpuQuotaBlock;
    /* 0x0208 */ unsigned long ReferenceCount;
    /* 0x020c */ unsigned long ProcessCount;
    /* 0x0210 */ struct _LIST_ENTRY QuotaList;
    /* 0x0220 */ long __PADDING__[8];
} EPROCESS_QUOTA_BLOCK, * PEPROCESS_QUOTA_BLOCK; /* size: 0x0240 */



typedef struct _OBJECT_HEADER
{
    /* 0x0000 */ __int64 PointerCount;
    union
    {
        /* 0x0008 */ __int64 HandleCount;
        /* 0x0008 */ void* NextToFree;
    }; /* size: 0x0008 */
    /* 0x0010 */ struct _EX_PUSH_LOCK Lock;
    /* 0x0018 */ unsigned char TypeIndex;
    union
    {
        /* 0x0019 */ unsigned char TraceFlags;
        struct /* bitfield */
        {
            /* 0x0019 */ unsigned char DbgRefTrace : 1; /* bit position: 0 */
            /* 0x0019 */ unsigned char DbgTracePermanent : 1; /* bit position: 1 */
        }; /* bitfield */
    }; /* size: 0x0001 */
    /* 0x001a */ unsigned char InfoMask;
    union
    {
        /* 0x001b */ unsigned char Flags;
        struct /* bitfield */
        {
            /* 0x001b */ unsigned char NewObject : 1; /* bit position: 0 */
            /* 0x001b */ unsigned char KernelObject : 1; /* bit position: 1 */
            /* 0x001b */ unsigned char KernelOnlyAccess : 1; /* bit position: 2 */
            /* 0x001b */ unsigned char ExclusiveObject : 1; /* bit position: 3 */
            /* 0x001b */ unsigned char PermanentObject : 1; /* bit position: 4 */
            /* 0x001b */ unsigned char DefaultSecurityQuota : 1; /* bit position: 5 */
            /* 0x001b */ unsigned char SingleHandleEntry : 1; /* bit position: 6 */
            /* 0x001b */ unsigned char DeletedInline : 1; /* bit position: 7 */
        }; /* bitfield */
    }; /* size: 0x0001 */
    /* 0x001c */ unsigned long Reserved;
    union
    {
        /* 0x0020 */ struct _OBJECT_CREATE_INFORMATION* ObjectCreateInfo;
        /* 0x0020 */ void* QuotaBlockCharged;
    }; /* size: 0x0008 */
    /* 0x0028 */ void* SecurityDescriptor;
    /* 0x0030 */ struct _QUAD Body;
} OBJECT_HEADER, * POBJECT_HEADER; /* size: 0x0038 */


#define OBJECT_TO_OBJECT_HEADER( o ) \
    CONTAINING_RECORD( (o), OBJECT_HEADER, Body )

#endif // !_NTOSDEF_H
