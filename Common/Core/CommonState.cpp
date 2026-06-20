#include "CommonInternal.h"

namespace Common
{
    HANDLE hMutex;
    bool isIntel = false;
    bool isAMD = false;
    std::mutex mutex;
}
