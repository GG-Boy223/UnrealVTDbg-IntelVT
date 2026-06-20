#pragma once

#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../AllocateMem.h"

namespace pool_manager
{
    bool allocate_pool_entries(unsigned __int64 size, unsigned __int32 count, allocation_intention intention);
}
