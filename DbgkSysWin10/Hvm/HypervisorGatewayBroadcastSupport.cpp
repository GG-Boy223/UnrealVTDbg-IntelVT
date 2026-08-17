#include "../Driver.h"
#include "../ntapi.h"
#include "AsmCallset.h"
#include "HypervisorGatewayInternal.h"

namespace hvgt
{
	void SignalDpcDone(PVOID SystemArgument1, PVOID SystemArgument2)
	{
		KeSignalCallDpcSynchronize(SystemArgument2);
		KeSignalCallDpcDone(SystemArgument1);
	}

	bool SafeVmCall(unsigned __int64 command,
		unsigned __int64 parameter1,
		unsigned __int64 parameter2,
		unsigned __int64 parameter3)
	{
		bool status = false;

		__try
		{
			status = __vm_call(command, parameter1, parameter2, parameter3);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = false;
		}

		return status;
	}

	bool SafeVmCallEx(unsigned __int64 command,
		unsigned __int64 parameter1,
		unsigned __int64 parameter2,
		unsigned __int64 parameter3,
		unsigned __int64 parameter4,
		unsigned __int64 parameter5,
		unsigned __int64 parameter6,
		unsigned __int64 parameter7,
		unsigned __int64 parameter8,
		unsigned __int64 parameter9)
	{
		bool status = false;

		__try
		{
			status = __vm_call_ex(command,
				parameter1,
				parameter2,
				parameter3,
				parameter4,
				parameter5,
				parameter6,
				parameter7,
				parameter8,
				parameter9);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = false;
		}

		return status;
	}

	bool TryReadVmcallCommand(PVOID vmcallinfo, unsigned __int64* command)
	{
		if (vmcallinfo == nullptr || command == nullptr)
		{
			return false;
		}

		__try
		{
			*command = ((PVMCALLINFO)vmcallinfo)->command;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			*command = 0;
			return false;
		}
	}
}
