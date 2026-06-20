#pragma once

#ifndef _NTLPCAPI_H
#define _NTLPCAPI_H

typedef struct _LPC_CLIENT_DIED_MSG
{
	/* 0x0000 */ struct _PORT_MESSAGE PortMsg;
	/* 0x0028 */ union _LARGE_INTEGER CreateTime;
} LPC_CLIENT_DIED_MSG, * PLPC_CLIENT_DIED_MSG; /* size: 0x0030 */


#endif // !_NTLPCAPI_H
