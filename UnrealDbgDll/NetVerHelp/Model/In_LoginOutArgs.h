#pragma once
#ifndef In_LoginOutArgs_H_
#define In_LoginOutArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	// 退出登录
	class In_LoginOutArgs :public In_DataBaseArgs
	{
	public:
		//登录令牌(必填)
		string token;
		// 卡密或账号(必填)
		string cardnumorusername;
	};
}
#endif