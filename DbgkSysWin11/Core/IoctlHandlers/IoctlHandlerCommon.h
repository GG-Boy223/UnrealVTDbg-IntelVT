#pragma once

#include <ntifs.h>
#include <Shared/SharedStruct.h>

USER_DATA GetUserData(PUSER_DATA userData);

UCHAR* AllocateAndDecryptUserRecords(const USER_DATA& user, size_t recordSize, size_t* recordCount);
