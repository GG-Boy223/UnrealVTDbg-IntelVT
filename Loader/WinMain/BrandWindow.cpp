#include "WinMainInternal.h"

// 定义窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		// 设置定时器
		SetTimer(hwnd, TIMER_TEXT, 500, NULL); // 每500毫秒更新一次

		// 用户选择“否”，正常执行符号下载
		skipDownload = false;
		InitThread(); // 启动符号下载线程

		break;
	}

	case WM_DESTROY:
	{
		//KillTimer(hwnd, TIMER_PROGRESSBAR); // 关闭定时器
		KillTimer(hwnd, TIMER_TEXT); // 关闭定时器
		PostQuitMessage(0);
		break;
	}
	case USER_PROGRESS_BAR:
	{
		// 更新进度条
		const WPARAM progressPercent = wParam > 100 ? 100 : wParam;
		progress = (nWidth * static_cast<int>(progressPercent)) / 100;
		if (progress > nWidth)
		{
			progress = 0; // 重置进度
		}
		displayText = L"下载" + modText + L"符号表(" + std::to_wstring(wParam) + L"%)";
		InvalidateRect(hwnd, NULL, TRUE); // 请求重绘
		break;
	}
	case WM_TIMER:
	{
		if (wParam == TIMER_TEXT)
		{
			if (progress == nWidth || progress == 0)
			{
				//环形计数器
				dotCount = (dotCount + 1) % (MAX_DOTS + 1);

				// 根据当前文本索引选择要显示的文本
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
		InvalidateRect(hwnd, NULL, TRUE); // 请求重绘
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		DrawBackground(hdc); //渲染背景
		DrawString(hwnd, hdc);
		DrawProgressBar(hwnd, hdc, ps); //渲染进度条
		EndPaint(hwnd, &ps);
		break;
	}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int DisplayBrand(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	// 注册窗口类
	const wchar_t CLASS_NAME[] = L"DisplayBrandClass";

	// 注册窗口类
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	//wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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

	// 授权校验
	//curdir += L"res\\mm.pak"; //old图片    //5F499EB6E77B203FA96DEB2A121FBA13
	curdir += L"res\\mm3.pak"; //新图片   // 92debbce51b96f9fd3078636a4aa9166

	if (!Common::fileExists(curdir) ||
		(_stricmp(calculateMD5(Common::wideStringToString2(curdir)).c_str(), "92debbce51b96f9fd3078636a4aa9166") != 0))
	{
		Common::ReportSeriousError("资源文件已损坏! (error: %d)", __func__, __LINE__, GetLastError());
		return 0;
	}

	// 加载图片
	HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, curdir.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	DWORD err = GetLastError();

	// 获取图片原始尺寸
	BITMAP bitmap;
	GetObject(hBitmap, sizeof(BITMAP), &bitmap);
	int originalWidth = bitmap.bmWidth;
	int originalHeight = bitmap.bmHeight;
	nWidth = bitmap.bmWidth;
	DeleteObject(hBitmap);

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 缩放或裁剪图片以适应屏幕
	int width, height, x, y;
	if (originalWidth > screenWidth || originalHeight > screenHeight)
	{
		// 图片尺寸大于屏幕尺寸，需要进行缩放或裁剪
		// 计算缩放比例
		float scaleWidth = (float)screenWidth / originalWidth;
		float scaleHeight = (float)screenHeight / originalHeight;
		float scale = min(scaleWidth, scaleHeight);

		// 缩放图片尺寸
		width = (int)(originalWidth * scale);
		height = (int)(originalHeight * scale);

		// 计算屏幕中心位置
		x = (screenWidth - width) / 2;
		y = (screenHeight - height) / 2;
	}
	else
	{
		// 图片尺寸小于等于屏幕尺寸，直接居中显示
		width = originalWidth;
		height = originalHeight;
		// 计算屏幕中心位置
		x = (screenWidth - width) / 2;
		y = (screenHeight - height) / 2;
	}

	// 创建窗口
	HWND hwnd = CreateWindowEx(
		0, // 扩展窗口样式
		CLASS_NAME, // 窗口类名
		L"", // 窗口标题
		WS_POPUP, // 窗口样式
		x, y, // 窗口位置
		width, height, // 窗口尺寸
		NULL, // 父窗口句柄
		NULL, // 菜单句柄
		hInstance, // 实例句柄
		NULL // 附加数据指针
	);

	if (hwnd == NULL)
	{
		return 0;
	}

	g_hwnd = hwnd;
	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	// 消息循环
	MSG msg = { 0 };

	while (1)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// test if this is a quit
			if (msg.message == WM_QUIT)
			{
				break;
			}

			// translate any accelerator keys
			TranslateMessage(&msg);
			// send the message to the window proc
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
