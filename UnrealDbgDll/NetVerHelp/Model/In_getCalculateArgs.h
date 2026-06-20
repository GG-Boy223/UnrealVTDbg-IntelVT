#pragma once
#ifndef In_getCalculateArgs_H_
#define In_getCalculateArgs_H_
#include <string>
#include "In_getCalculateArgs.h"
using namespace std;
namespace Model
{
	// 获取远程算法入参
	class In_getCalculateArgs :public In_DataBaseArgs
	{
	public:
		//登录令牌(必填)
		string token;
		// 算法ID(后台->防破管理->远程算法列表->算法ID)
		string calculateid;
		//请求转发地址提交的参数(参数格式示例：args1=value1&args2=value2)
		string requestargs;
	};
}
#endif