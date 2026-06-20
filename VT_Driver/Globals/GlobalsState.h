#pragma once

#ifndef _GLOBALS_STATE_H
#define _GLOBALS_STATE_H

#include "GlobalsStructures.h"

extern PFN_PSGETNEXTPROCESS PsGetNextProcess; // PsGetNextProcess 全局指针


extern __pseudo_descriptor64 g_gdtr; // 保存的 GDTR
extern __pseudo_descriptor64 g_idtr; // 保存的 IDTR
extern unsigned __int64 g_guest_cr0; // 保存的 guest CR0
extern unsigned __int64 g_guest_cr3; // 保存的 guest CR3
extern unsigned __int64 g_guest_cr4; // 保存的 guest CR4
extern __vmm_context g_vmm_context; // 全局 VMM 上下文
extern uint16_t guest_vpid; // guest VPID
extern volatile long eptWatchList_lock; // 监视表自旋锁
extern EPTWatchEntry eptWatchList[EPTWATCHLISTSIZE]; // 监视断点表

namespace ethread_offset // ETHREAD 结构偏移集合
{
    extern size_t Tcb; // ETHREAD.Tcb 偏移
    extern size_t CrossThreadFlags; // ETHREAD.CrossThreadFlags 偏移
    extern size_t Cid; // ETHREAD.Cid 偏移
    extern size_t RundownProtect; // ETHREAD.RundownProtect 偏移
}

#endif // !_GLOBALS_STATE_H
