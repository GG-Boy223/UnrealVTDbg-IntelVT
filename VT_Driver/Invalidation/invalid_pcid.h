#pragma once

#ifndef _INVALID_PCID_H
#define _INVALID_PCID_H

struct __invpcid_descriptor
{
	unsigned __int64 pcid : 12;
	unsigned __int64 reserved : 52;
	unsigned __int64 linear_address;
};

#endif // !_INVALID_PCID_H
