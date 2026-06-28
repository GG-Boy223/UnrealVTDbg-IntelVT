#include "WinMainInternal.h"

void DrawBackground(HDC hdc)
{
	// 加载图片
	HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, curdir.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

	// 获取图片原始尺寸
	BITMAP bitmap;
	GetObject(hBitmap, sizeof(BITMAP), &bitmap);
	int width = bitmap.bmWidth;
	int height = bitmap.bmHeight;
	int x = 0;
	int y = 0;

	// 绘制图片
	HDC memDC = CreateCompatibleDC(hdc);
	SelectObject(memDC, hBitmap);
	BitBlt(hdc, x, y, width, height, memDC, 0, 0, SRCCOPY);

	// 释放资源
	DeleteDC(memDC);
	DeleteObject(hBitmap);
}

void DrawProgressBar(HWND hwnd, HDC hdc, PAINTSTRUCT ps)
{
	// 绘制进度条
	RECT rect;
	GetClientRect(hwnd, &rect);
	rect.top = rect.bottom - 5; // 进度条的顶部位置
	//rect.bottom -= 10; // 进度条的底部位置
	rect.right = rect.left + progress; // 根据当前进度调整宽度

	// 创建绿色画刷
	HBRUSH hGreenBrush = CreateSolidBrush(RGB(0, 255, 0)); // 创建绿色画刷
	FillRect(hdc, &rect, hGreenBrush); // 使用绿色填充进度条

	DeleteObject(hGreenBrush); // 删除画刷
}

void DrawString(HWND hwnd, HDC hdc)
{
	// 设置文本颜色和背景颜色
	SetTextColor(hdc, RGB(255, 255, 255)); // 白色
	SetBkMode(hdc, TRANSPARENT);

	// 创建字体
	HFONT hFont = CreateFont(
		20, // 字体高度
		0, // 字体宽度
		0, // 旋转角度
		0, // 基线角度
		FW_NORMAL, // 字体粗细
		FALSE, // 斜体
		FALSE, // 下划线
		FALSE, // 删除线
		DEFAULT_CHARSET, // 字符集
		OUT_DEFAULT_PRECIS, // 外部精度
		CLIP_DEFAULT_PRECIS, // 剪裁精度
		DEFAULT_QUALITY, // 质量
		DEFAULT_QUALITY, // 字体质量
		L"宋体" // 字体名称
	);

	// 选择字体到设备上下文
	SelectObject(hdc, hFont);

	// 绘制文字
	RECT rect;
	GetClientRect(hwnd, &rect);

	// 获取文本的宽度和高度
	SIZE textSize;
	const int displayTextLength =
		displayText.length() > INT_MAX ? INT_MAX : static_cast<int>(displayText.length());
	GetTextExtentPoint32(hdc, displayText.c_str(), displayTextLength, &textSize);

	// 计算绘制位置，使文本右对齐
	int x = rect.right - textSize.cx; // 右侧位置
	TextOut(hdc, x, rect.bottom - 50, displayText.c_str(), displayTextLength);
	DeleteObject(hFont);
}
