#include "Core/DllMainInternal.h"

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,
    DWORD fdwReason,
    LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    BOOL bRet = TRUE;

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hinstDLL);
        break;
    }

    case DLL_THREAD_ATTACH:
    {
        break;
    }

    case DLL_THREAD_DETACH:
    {
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        break;
    }

    default:
    {
        break;
    }
    }

    return bRet;
}
