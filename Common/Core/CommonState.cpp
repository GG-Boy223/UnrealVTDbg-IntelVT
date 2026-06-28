#include "CommonInternal.h"

namespace Common
{
    HANDLE hMutex; // 防多开
    bool isIntel = false;
    bool isAMD = false;
    std::mutex mutex; // 互斥锁
}
