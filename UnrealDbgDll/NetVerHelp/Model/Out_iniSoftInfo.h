#pragma once
#ifndef Out_iniSoftInfo_H_
#define Out_iniSoftInfo_H_
#include <string>
#include <list>
#include "Out_softInfo.h"
#include "Out_DataBaseArgs.h"
using namespace std;
namespace Model
{
	//软件初始化的data数据
	class  Out_iniSoftInfo :public Out_DataBaseArgs
	{
	public:
		//初始化软件Key值(通过软件初始化接口【iniSoftInfo】获取)
		string inisoftkey;
		//软件信息
		Out_softInfo softInfo;
	};
}
#endif