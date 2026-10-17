#include <Windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include <tchar.h>
#include <vector>
#include <sstream>
#include <mutex>
#include "../Common.h"
#include "../FileSystem/FileSystem.h"
#include "Logger.h"

PFN_PRINTLOG pfnPrintLog;