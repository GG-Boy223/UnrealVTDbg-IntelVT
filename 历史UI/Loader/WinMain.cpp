#include "WinMain/WinMainInternal.h"

//启动程序
int CALLBACK WinMain(
	_In_           HINSTANCE hInstance,
	_In_opt_       HINSTANCE hPrevInstance,
	_In_           LPSTR     lpCmdLine,
	_In_           int       nShowCmd
)
{
	if (DisplayBrand(hInstance, hPrevInstance, lpCmdLine, nShowCmd))
	{
		StartProcess();
	}
	return 0;
}
