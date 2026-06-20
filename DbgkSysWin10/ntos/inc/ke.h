#pragma once

#ifndef _KE_H
#define _KE_H

EXTERN_C
NTSYSAPI
VOID
NTAPI
KeStackAttachProcess(IN PRKPROCESS Process, OUT PRKAPC_STATE ApcState);

EXTERN_C
NTSYSAPI
VOID
NTAPI
KeUnstackDetachProcess(IN PRKAPC_STATE ApcState);

BOOLEAN
KeReadStateThread(
    IN PKTHREAD Thread
);

#endif // !_KE_H
