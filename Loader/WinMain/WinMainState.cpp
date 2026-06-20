#include "WinMainInternal.h"

int dotCount = 0;
std::wstring baseText1(L"正在初始化程序");
std::wstring modText;
std::wstring displayText;
int currentTextIndex = 0;

int nWidth = 0;
int progress = 0;
int tickcount = 0;

bool skipDownload = false;

HWND g_hwnd;
std::wstring curdir;
