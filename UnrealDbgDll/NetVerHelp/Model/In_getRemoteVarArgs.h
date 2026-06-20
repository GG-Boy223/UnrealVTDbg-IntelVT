#pragma once
#ifndef In_getRemoteVarArgs_H_
#define In_getRemoteVarArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	// 获取远程变量入参
	class In_getRemoteVarArgs :public In_DataBaseArgs
	{
	public:
		//登录令牌(必填)
		string token;
		// 卡密或账号(必填)
		string cardnumorusername;
		//变量名称(必填) 如果变量名称为空的话，那么接口返回来的是此软件所有变量，否则就是此变量名所以应的变量值
		string varname;
	};
}
#endif