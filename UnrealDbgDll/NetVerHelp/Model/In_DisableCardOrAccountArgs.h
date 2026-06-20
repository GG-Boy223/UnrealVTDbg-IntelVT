#pragma once
#ifndef DisableCardOrAccountArgs_H_
#define DisableCardOrAccountArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
#include "DisableCardOrAccountType.h"
using namespace std;
namespace Model
{
	// 禁用还是删除：单码或账号
	class In_DisableCardOrAccountArgs :public In_DataBaseArgs
	{
	public:
		//登录令牌(必填)
		string token;
		// 卡密或账号(必填)
		string cardnumorusername;
		//禁用还是删除：单码或账号(必填)
		DisableCardOrAccountType disablecardoraccounttype;
	};
}
#endif
