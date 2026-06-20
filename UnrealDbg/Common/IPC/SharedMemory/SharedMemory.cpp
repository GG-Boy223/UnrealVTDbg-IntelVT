#include <iostream>
#include <string>
#include <Windows.h>
#include "SharedMemory.h"

SharedMemory* InitializeSharedMemory(const std::wstring& name)
{
	return new SharedMemory(name, sizeof(SHARED_DATA));
}