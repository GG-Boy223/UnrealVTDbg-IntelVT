#pragma once

struct __vcpu;

namespace VmcallDispatch
{
	struct Request
	{
		unsigned __int64 reason;
		unsigned __int64 parameter1;
		unsigned __int64 parameter2;
		unsigned __int64 parameter3;
		unsigned __int64 parameter4;
		unsigned __int64 parameter5;
		unsigned __int64 parameter6;
		unsigned __int64 parameter7;
		unsigned __int64 parameter8;
		unsigned __int64 parameter9;
	};

	bool Execute(__vcpu* vcpu, Request const& request);
}
