#include <Windows.h>
#include <mutex>
#include <string>
#include <vector>
#include <tuple>
#include <cstdint>
#include "Common.h"

namespace Common
{
	HANDLE hMutex;
	bool isIntel = false;
	bool isAMD = false;
	std::mutex mutex;
}
