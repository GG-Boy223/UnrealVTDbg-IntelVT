#pragma once

#include <ntifs.h>
#include <Shared/SharedStruct.h>

UCHAR* AllocateAndDecryptUserRecords(const USER_DATA& User, size_t RecordSize, size_t* RecordCount);
