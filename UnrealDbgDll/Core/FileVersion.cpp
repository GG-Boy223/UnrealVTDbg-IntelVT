#include "DllMainInternal.h"

void GetFileVersion(_In_ TCHAR* FileName, _Out_ TCHAR* VerInfo)
{
    VS_FIXEDFILEINFO* pVsInfo;
    UINT iFileInfoSize = sizeof(VS_FIXEDFILEINFO);

    DWORD iVerInfoSize = GetFileVersionInfoSize(FileName, NULL);
    if (iVerInfoSize)
    {
        TCHAR* pBuf = new TCHAR[iVerInfoSize];
        if (pBuf)
        {
            if (GetFileVersionInfo(FileName, 0, iVerInfoSize, pBuf))
            {
                if (VerQueryValue(pBuf, _T("\\"), (LPVOID*)&pVsInfo, &iFileInfoSize))
                {
                    wsprintf(pBuf, _T("%d.%d.%d.%d"), HIWORD(pVsInfo->dwFileVersionMS), LOWORD(pVsInfo->dwFileVersionMS), HIWORD(pVsInfo->dwFileVersionLS), LOWORD(pVsInfo->dwFileVersionLS));
                    wcscpy(VerInfo, pBuf);
                }
            }
            delete[] pBuf;
        }
    }
}
