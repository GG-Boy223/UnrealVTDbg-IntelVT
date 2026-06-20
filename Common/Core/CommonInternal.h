#pragma once

#include <Windows.h>
#include <string>
#include <random>
#include <tuple>
#include <TlHelp32.h>
#include <vector>
#include <psapi.h>
#include <intrin.h>
#include <array>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "../Common.h"

namespace Common
{
    extern HANDLE hMutex;
    extern std::mutex mutex;
}
