#pragma once

#ifndef _VMP_H
#define _VMP_H

#include "VMProtectSDK.h"

class VMProtectionScope {
public:
	VMProtectionScope() { VMProtectBeginVirtualization("VMP"); }
	~VMProtectionScope() { VMProtectEnd(); }
};


#endif // !_VMP_H
