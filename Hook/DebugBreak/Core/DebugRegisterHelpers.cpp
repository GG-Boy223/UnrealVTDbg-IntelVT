#include "DebugBreakInternal.h"

namespace DebugBreakInternal
{
    ULONG64 ReadDebugRegister(const CONTEXT* context, int debugRegister)
    {
        switch (debugRegister)
        {
        case 0: return context->Dr0;
        case 1: return context->Dr1;
        case 2: return context->Dr2;
        case 3: return context->Dr3;
        default: return 0;
        }
    }

    void WriteDebugRegister(CONTEXT* context, int debugRegister, ULONG64 value)
    {
        switch (debugRegister)
        {
        case 0: context->Dr0 = value; break;
        case 1: context->Dr1 = value; break;
        case 2: context->Dr2 = value; break;
        case 3: context->Dr3 = value; break;
        default: break;
        }
    }

    bool IsDebugRegisterEnabled(ULONG64 dr7, int debugRegister)
    {
        return ((dr7 >> (debugRegister * 2)) & 0x3) != 0;
    }

    int GetDebugRegisterRw(ULONG64 dr7, int debugRegister)
    {
        return static_cast<int>((dr7 >> (16 + debugRegister * 4)) & 0x3);
    }

    int GetDebugRegisterLenCode(ULONG64 dr7, int debugRegister)
    {
        return static_cast<int>((dr7 >> (18 + debugRegister * 4)) & 0x3);
    }

    bool HasEnabledDebugRegister(const CONTEXT* context)
    {
        if (context == nullptr)
        {
            return false;
        }

        const ULONG64 dr7 = context->Dr7;
        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            if (IsDebugRegisterEnabled(dr7, i))
            {
                return true;
            }
        }
        return false;
    }

    int DecodeDebugRegisterLength(int lenCode)
    {
        switch (lenCode)
        {
        case BYTE_1: return 1;
        case BYTE_2: return 2;
        case BYTE_4: return 4;
        case BYTE_8: return 8;
        default: return 1;
        }
    }

    bool DecodeDebugRegisterCommand(int rwCode, unsigned __int64* command)
    {
        switch (rwCode)
        {
        case WATCH_EXECUTION_ONLY:
            return false;
        case WATCH_WRITE:
            *command = VMCALL_WATCH_WRITES;
            return true;
        case WATCH_READWRITE:
            *command = VMCALL_WATCH_READS;
            return true;
        default:
            return false;
        }
    }
}
