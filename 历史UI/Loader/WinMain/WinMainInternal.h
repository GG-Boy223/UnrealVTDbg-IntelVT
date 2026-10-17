#pragma once

#include "../WinMain.h"
#include "../Symbols.h"

constexpr int TIMER_PROGRESSBAR = 1;
constexpr int TIMER_TEXT = 2;
constexpr int MAX_DOTS = 3;

extern int dotCount;
extern std::wstring baseText1;
extern std::wstring modText;
extern std::wstring displayText;
extern int currentTextIndex;
extern int nWidth;
extern int progress;
extern int tickcount;
extern bool skipDownload;
extern std::wstring curdir;

void DrawBackground(HDC hdc);
void DrawProgressBar(HWND hwnd, HDC hdc, PAINTSTRUCT ps);
void DrawString(HWND hwnd, HDC hdc);
void DownloadSymbol();
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int DisplayBrand(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd);
PROCESS_INFORMATION _StartProcess_(PSTARTUP_INFO pStartInfo);
PROCESS_INFORMATION StartProcess_internal(std::wstring processPath, std::wstring procName, std::wstring sCommandLine);
void StartProcess();
