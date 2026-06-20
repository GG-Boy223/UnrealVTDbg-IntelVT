#pragma once
#ifndef In_IniSoftInfoArgs_H_
#define In_IniSoftInfoArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	//软件初始化入参
	class In_IniSoftInfoArgs :public In_DataBaseArgs
	{

	public:
		// 软件版本号(必填)
		// 注意:填写您软件当前的版本号
		string versionname;
	};
}
#endif