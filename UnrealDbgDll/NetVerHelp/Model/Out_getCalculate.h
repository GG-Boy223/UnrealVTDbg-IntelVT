#pragma once
#ifndef Out_getCalculate_H_
#define Out_getCalculate_H_
#include <string>
#include "Out_DataBaseArgs.h"
using namespace std;
namespace Model
{
	//远程算法出参
	class Out_getCalculate :public Out_DataBaseArgs
	{
	public:
		// 远程算法转发地址返回的数据
		string calculateresult;
	};
}
#endif