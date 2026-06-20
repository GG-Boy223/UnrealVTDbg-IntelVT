#include "WinMainInternal.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		SetTimer(hwnd, TIMER_TEXT, 500, NULL);

		skipDownload = false;
		InitThread();

		break;
	}

	case WM_DESTROY:
	{
		KillTimer(hwnd, TIMER_TEXT);
		PostQuitMessage(0);
		break;
	}
	case USER_PROGRESS_BAR:
	{
		const WPARAM progressPercent = wParam > 100 ? 100 : wParam;
		progress = (nWidth * static_cast<int>(progressPercent)) / 100;
		if (progress > nWidth)
		{
			progress = 0;
		}
		displayText = L"下载" + modText + L"符号表(" + std::to_wstring(wParam) + L"%)";
		InvalidateRect(hwnd, NULL, TRUE);
		break;
	}
	case WM_TIMER:
	{
		if (wParam == TIMER_TEXT)
		{
			if (progress == nWidth || progress == 0)
			{
				dotCount = (dotCount + 1) % (MAX_DOTS + 1);

				std::wstring dots(dotCount, L'.');
				displayText = baseText1 + dots;

				if (skipDownload)
				{
					tickcount++;
				}
				else if (progress == nWidth)
				{
					tickcount++;
				}
			}
		}
		else if (wParam == TIMER_PROGRESSBAR)
		{
		}
		InvalidateRect(hwnd, NULL, TRUE);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		DrawBackground(hdc);
		DrawString(hwnd, hdc);
		DrawProgressBar(hwnd, hdc, ps);
		EndPaint(hwnd, &ps);
		break;
	}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int DisplayBrand(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	const wchar_t CLASS_NAME[] = L"DisplayBrandClass";

	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = CLASS_NAME;
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		return 0;
	}

	curdir = FileSystem::GetModuleDirectory(NULL);
	if (curdir.empty())
	{
		Common::ReportSeriousError("%s[%d] 获取程序目录失败! (error: %d)", __func__, __LINE__, GetLastError());
		return 0;
	}

	curdir += L"res\\mm3.pak";

	if (!Common::fileExists(curdir) ||
		(_stricmp(calculateMD5(Common::wideStringToString2(curdir)).c_str(), "92debbce51b96f9fd3078636a4aa9166") != 0))
	{
		Common::ReportSeriousError("资源文件已损坏! (error: %d)", __func__, __LINE__, GetLastError());
		return 0;
	}

	HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, curdir.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	DWORD err = GetLastError();

	BITMAP bitmap;
	GetObject(hBitmap, sizeof(BITMAP), &bitmap);
	int originalWidth = bitmap.bmWidth;
	int originalHeight = bitmap.bmHeight;
	nWidth = bitmap.bmWidth;
	DeleteObject(hBitmap);

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	int width, height, x, y;
	if (originalWidth > screenWidth || originalHeight > screenHeight)
	{
		float scaleWidth = (float)screenWidth / originalWidth;
		float scaleHeight = (float)screenHeight / originalHeight;
		float scale = min(scaleWidth, scaleHeight);

		width = (int)(originalWidth * scale);
		height = (int)(originalHeight * scale);

		x = (screenWidth - width) / 2;
		y = (screenHeight - height) / 2;
	}
	else
	{
		width = originalWidth;
		height = originalHeight;
		x = (screenWidth - width) / 2;
		y = (screenHeight - height) / 2;
	}

	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"",
		WS_POPUP,
		x, y,
		width, height,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd == NULL)
	{
		return 0;
	}

	g_hwnd = hwnd;
	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	MSG msg = { 0 };

	while (1)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (tickcount > 10)
		{
			DestroyWindow(hwnd);
			return 1;
		}
	}
	return 0;
}
