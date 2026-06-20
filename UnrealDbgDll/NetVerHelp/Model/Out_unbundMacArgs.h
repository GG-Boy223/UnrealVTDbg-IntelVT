#pragma once
#ifndef Out_unbundMacArgs_H_
#define Out_unbundMacArgs_H_
#include <string>
#include "Out_DataBaseArgs.h"
using namespace std;
namespace Model
{
	// 解绑机器码出参
	class Out_unbundMacArgs :public Out_DataBaseArgs
	{
	public:
		//账号到期时间
		string endtime;
		// 剩余点数
		int surpluspointvalue;
	};
}
#endif