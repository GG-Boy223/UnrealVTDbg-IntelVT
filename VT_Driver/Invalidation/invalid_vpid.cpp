#include "Driver.h"
#include "invalid_vpid.h"
#include "AsmCallset.h"

void invvpid_invidual_address_func(unsigned __int64 linear_address, unsigned __int16 vpid)
{
	__invvpid_descriptor descriptor = { 0 };
	descriptor.linear_address = linear_address;
	descriptor.vpid = vpid;

	__invvpid(invvpid_individual_address, &descriptor);
}

void invvpid_single_context_func(unsigned __int16 vpid)
{
	__invvpid_descriptor descriptor = { 0 };
	descriptor.vpid = vpid;

	__invvpid(invvpid_single_context, &descriptor);
}

void invvpid_all_contexts()
{
	__invvpid_descriptor descriptor = { 0 };
	__invvpid(invvpid_all_context, &descriptor);
}

void invvpid_single_context_except_global_translations(unsigned __int16 vpid)
{
	__invvpid_descriptor descriptor = { 0 };
	descriptor.vpid = vpid;
	return __invvpid(invvpid_single_context_retaining_globals, &descriptor);
}
