#pragma once

#ifndef _KI_H
#define _KI_H

#define BREAKPOINT_BREAK 0
#define BREAKPOINT_PRINT 1
#define BREAKPOINT_PROMPT 2
#define BREAKPOINT_LOAD_SYMBOLS 3
#define BREAKPOINT_UNLOAD_SYMBOLS 4
#define BREAKPOINT_COMMAND_STRING 5


BOOLEAN
FORCEINLINE
KdIsThisAKdTrap(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN KPROCESSOR_MODE PreviousMode
)

/*++

Routine Description:

    This routine is called whenever a user mode exception occurs and
    it might be a kernel debugger exception (e.g., DbgPrint/DbgPrompt).

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record that
        describes the exception.

    ContextRecord - Supplies the context at the time of the exception.

    PreviousMode - Supplies the previous processor mode.

Return Value:

    A value of TRUE is returned if this is for the kernel debugger.
    Otherwise, a value of FALSE is returned.

--*/

{

    if ((ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT) &&
        (ExceptionRecord->NumberParameters > 0) &&
        (ExceptionRecord->ExceptionInformation[0] != BREAKPOINT_BREAK))
    {
        return TRUE;

    }
    else
    {
        return FALSE;
    }
}

EXTERN_C
VOID KiDispatchException(IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN KPROCESSOR_MODE PreviousMode,
    IN BOOLEAN FirstChance);


#endif // !_KI_H
