#include "Globals/GlobalsInternal.h"

__pseudo_descriptor64 g_gdtr = { 0 };
__pseudo_descriptor64 g_idtr = { 0 };
unsigned __int64 g_guest_cr0;
unsigned __int64 g_guest_cr3;
unsigned __int64 g_guest_cr4;
void* g_vt_driver_image_base;
unsigned __int64 g_vt_driver_image_size;
__vmm_context g_vmm_context;
uint16_t guest_vpid = 1;
int eptWatchListSize;
int eptWatchListPos;
volatile long eptWatchList_lock = 0;

EPTWatchEntry eptWatchList[EPTWATCHLISTSIZE];

PFN_PSGETNEXTPROCESS PsGetNextProcess;

namespace ethread_offset
{
    size_t Tcb;
    size_t CrossThreadFlags;
    size_t Cid = static_cast<size_t>(0xFFFFFFFF);
    size_t RundownProtect;
}

namespace hv
{
    segment_selector host_cs_selector = { 0, 0, 1 };
    segment_selector host_tr_selector = { 0, 0, 2 };
    uint64_t host_physical_memory_pml4_idx = 255;
    uint8_t* host_physical_memory_base = reinterpret_cast<uint8_t*>((uint64_t)255 << (9 + 9 + 9 + 12));
    hypervisor ghv;
}
