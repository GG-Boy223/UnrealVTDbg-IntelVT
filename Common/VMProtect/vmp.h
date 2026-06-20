#pragma once

#ifndef _VMP_H
#define _VMP_H

#ifdef UNREALVTDBG_NO_VMP_SDK_IMPORT
inline void VMProtectBeginVirtualization(const char*)
{
}

inline void VMProtectEnd()
{
}
#else
#include "VMProtectSDK.h"
#endif

class VMProtectionScope {
public:
	VMProtectionScope() { VMProtectBeginVirtualization("VMP"); }
	~VMProtectionScope() { VMProtectEnd(); }
};


#endif // !_VMP_H
