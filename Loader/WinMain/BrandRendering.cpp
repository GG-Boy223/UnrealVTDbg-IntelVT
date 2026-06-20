#include "WinMainInternal.h"

void DrawBackground(HDC hdc)
{
	HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, curdir.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

	BITMAP bitmap;
	GetObject(hBitmap, sizeof(BITMAP), &bitmap);
	int width = bitmap.bmWidth;
	int height = bitmap.bmHeight;
	int x = 0;
	int y = 0;

	HDC memDC = CreateCompatibleDC(hdc);
	SelectObject(memDC, hBitmap);
	BitBlt(hdc, x, y, width, height, memDC, 0, 0, SRCCOPY);

	DeleteDC(memDC);
	DeleteObject(hBitmap);
}

void DrawProgressBar(HWND hwnd, HDC hdc, PAINTSTRUCT ps)
{
	RECT rect;
	GetClientRect(hwnd, &rect);
	rect.top = rect.bottom - 5;
	rect.right = rect.left + progress;

	HBRUSH hGreenBrush = CreateSolidBrush(RGB(0, 255, 0));
	FillRect(hdc, &rect, hGreenBrush);

	DeleteObject(hGreenBrush);
}

void DrawString(HWND hwnd, HDC hdc)
{
	SetTextColor(hdc, RGB(255, 255, 255));
	SetBkMode(hdc, TRANSPARENT);

	HFONT hFont = CreateFont(
		20,
		0,
		0,
		0,
		FW_NORMAL,
		FALSE,
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,
		DEFAULT_QUALITY,
		L"宋体"
	);

	SelectObject(hdc, hFont);

	RECT rect;
	GetClientRect(hwnd, &rect);

	SIZE textSize;
	const int displayTextLength =
		displayText.length() > INT_MAX ? INT_MAX : static_cast<int>(displayText.length());
	GetTextExtentPoint32(hdc, displayText.c_str(), displayTextLength, &textSize);

	int x = rect.right - textSize.cx;
	TextOut(hdc, x, rect.bottom - 50, displayText.c_str(), displayTextLength);
	DeleteObject(hFont);
}
