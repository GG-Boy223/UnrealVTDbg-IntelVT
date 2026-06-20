#pragma once
#ifndef In_HeartBeatArgs_H_
#define In_HeartBeatArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	// 心跳入参
	class In_HeartBeatArgs : public In_DataBaseArgs
	{
	public:
		//登录令牌(必填)
		string token;
		//心跳Key,每次心跳此值都会变
		string heartbeatkey;
		// 卡密或账号(必填)
		string cardnumorusername;
	};
}
#endif