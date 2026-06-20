#pragma once
#ifndef In_unbundMacArgs_H_
#define In_unbundMacArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	// 解绑机器码入参
	class In_unbundMacArgs :public In_DataBaseArgs
	{
	public:
		// 卡密或账号(必填)
		string cardnumorusername;
	};
}
#endif