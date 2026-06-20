#pragma once

#ifndef _PSTYPES_H
#define _PSTYPES_H

#define PS_MISC_FLAGS_SYSTEM_THREAD               0x00002000UL


#define PS_JOB_STATUS_NOT_REALLY_ACTIVE      0x00000001UL
#define PS_JOB_STATUS_ACCOUNTING_FOLDED      0x00000002UL
#define PS_JOB_STATUS_NEW_PROCESS_REPORTED   0x00000004UL
#define PS_JOB_STATUS_EXIT_PROCESS_REPORTED  0x00000008UL
#define PS_JOB_STATUS_REPORT_COMMIT_CHANGES  0x00000010UL
#define PS_JOB_STATUS_LAST_REPORT_MEMORY     0x00000020UL
#define PS_JOB_STATUS_REPORT_PHYSICAL_PAGE_CHANGES  0x00000040UL


//
// Process flags. Use interlocked operations with PS_SET_BITS, etc
// to modify these.
//

#define PS_PROCESS_FLAGS_CREATE_REPORTED        0x00000001UL // Create process debug call has occurred
#define PS_PROCESS_FLAGS_NO_DEBUG_INHERIT       0x00000002UL // Don't inherit debug port
#define PS_PROCESS_FLAGS_PROCESS_EXITING        0x00000004UL // PspExitProcess entered
#define PS_PROCESS_FLAGS_PROCESS_DELETE         0x00000008UL // Delete process has been issued
#define PS_PROCESS_FLAGS_WOW64_SPLIT_PAGES      0x00000010UL // Wow64 split pages
#define PS_PROCESS_FLAGS_VM_DELETED             0x00000020UL // VM is deleted
#define PS_PROCESS_FLAGS_OUTSWAP_ENABLED        0x00000040UL // Outswap enabled
#define PS_PROCESS_FLAGS_OUTSWAPPED             0x00000080UL // Outswapped
#define PS_PROCESS_FLAGS_FORK_FAILED            0x00000100UL // Fork status
#define PS_PROCESS_FLAGS_WOW64_4GB_VA_SPACE     0x00000200UL // Wow64 process with 4gb virtual address space
#define PS_PROCESS_FLAGS_ADDRESS_SPACE1         0x00000400UL // Addr space state1
#define PS_PROCESS_FLAGS_ADDRESS_SPACE2         0x00000800UL // Addr space state2
#define PS_PROCESS_FLAGS_SET_TIMER_RESOLUTION   0x00001000UL // SetTimerResolution has been called
#define PS_PROCESS_FLAGS_BREAK_ON_TERMINATION   0x00002000UL // Break on process termination
#define PS_PROCESS_FLAGS_CREATING_SESSION       0x00004000UL // Process is creating a session
#define PS_PROCESS_FLAGS_USING_WRITE_WATCH      0x00008000UL // Process is using the write watch APIs
#define PS_PROCESS_FLAGS_IN_SESSION             0x00010000UL // Process is in a session
#define PS_PROCESS_FLAGS_OVERRIDE_ADDRESS_SPACE 0x00020000UL // Process must use native address space (Win64 only)
#define PS_PROCESS_FLAGS_HAS_ADDRESS_SPACE      0x00040000UL // This process has an address space
#define PS_PROCESS_FLAGS_LAUNCH_PREFETCHED      0x00080000UL // Process launch was prefetched
#define PS_PROCESS_INJECT_INPAGE_ERRORS         0x00100000UL // Process should be given inpage errors - hardcoded in trap.asm too
#define PS_PROCESS_FLAGS_VM_TOP_DOWN            0x00200000UL // Process memory allocations default to top-down
#define PS_PROCESS_FLAGS_IMAGE_NOTIFY_DONE      0x00400000UL // We have sent a message for this image
#define PS_PROCESS_FLAGS_PDE_UPDATE_NEEDED      0x00800000UL // The system PDEs need updating for this process (NT32 only)
#define PS_PROCESS_FLAGS_VDM_ALLOWED            0x01000000UL // Process allowed to invoke NTVDM support
#define PS_PROCESS_FLAGS_SMAP_ALLOWED           0x02000000UL // Process allowed to invoke SMAP support
#define PS_PROCESS_FLAGS_CREATE_FAILED          0x04000000UL // Process create failed
#define PS_PROCESS_FLAGS_DEFAULT_IO_PRIORITY    0x38000000UL // The default I/O priority for created threads. (3 bits)
#define PS_PROCESS_FLAGS_PRIORITY_SHIFT         27
#define PS_PROCESS_FLAGS_EXECUTE_SPARE1         0x40000000UL //
#define PS_PROCESS_FLAGS_EXECUTE_SPARE2         0x80000000UL //


#define PS_PROCESS_FLAGS2_PROTECTED_PROCESS     0x00000800UL // ProtectedProcess
#define PS_PROCESS_FLAGS2_HANDLE_TABLE_RUNDOWN  0x00000080UL
#define PS_PROCESS_FLAGS2_STACK_RANDOMIZATION_DISABLED 0x00008000UL


#define KPROCESS_AUTO_ALIGNMENT_BIT 0
#define KPROCESS_DISABLE_BOOST_BIT 1
#define KPROCESS_DISABLE_QUANTUM_BIT 2


#define PROCESS_CREATE_FLAGS_BREAKAWAY               0x00000001
#define PROCESS_CREATE_FLAGS_NO_DEBUG_INHERIT        0x00000002
#define PROCESS_CREATE_FLAGS_INHERIT_HANDLES         0x00000004
#define PROCESS_CREATE_FLAGS_OVERRIDE_ADDRESS_SPACE  0x00000008
#define PROCESS_CREATE_FLAGS_LARGE_PAGES             0x00000010


//
// Extended Limits
//
#define JOB_OBJECT_LIMIT_PROCESS_MEMORY             0x00000100
#define JOB_OBJECT_LIMIT_JOB_MEMORY                 0x00000200
#define JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION 0x00000400
#define JOB_OBJECT_LIMIT_BREAKAWAY_OK               0x00000800
#define JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK        0x00001000
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE          0x00002000


#endif // !_PSTYPES_H
