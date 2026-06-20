#pragma once

#ifndef _NTI386_H
#define _NTI386_H

//
// The following values specify the type of failing access when the status is 
// STATUS_ACCESS_VIOLATION and the first parameter in the execpetion record.
//

#define EXCEPTION_READ_FAULT          0 // Access violation was caused by a read
#define EXCEPTION_WRITE_FAULT         1 // Access violation was caused by a write
#define EXCEPTION_EXECUTE_FAULT       8 // Access violation was caused by an instruction fetch

//
// i386 Feature bit definitions
//
// N.B. The no execute feature flags must be identical on all platforms.

#define KF_V86_VIS          0x00000001
#define KF_RDTSC            0x00000002
#define KF_CR4              0x00000004
#define KF_CMOV             0x00000008
#define KF_GLOBAL_PAGE      0x00000010
#define KF_LARGE_PAGE       0x00000020
#define KF_MTRR             0x00000040
#define KF_CMPXCHG8B        0x00000080
#define KF_MMX              0x00000100
#define KF_WORKING_PTE      0x00000200
#define KF_PAT              0x00000400
#define KF_FXSR             0x00000800
#define KF_FAST_SYSCALL     0x00001000
#define KF_XMMI             0x00002000
#define KF_3DNOW            0x00004000
#define KF_AMDK6MTRR        0x00008000
#define KF_XMMI64           0x00010000
#define KF_DTS              0x00020000
#define KF_NOEXECUTE        0x20000000
#define KF_GLOBAL_32BIT_EXECUTE 0x40000000
#define KF_GLOBAL_32BIT_NOEXECUTE 0x80000000

#endif // !_NTI386_H
