#pragma once
#ifndef In_CardLoginArgs_H_
#define In_CardLoginArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	/*请求的数据类：卡密登录*（登陆类）*/
	// /*请求的数据类：卡密登录*（登陆类）*/
	class In_CardLoginArgs :public In_DataBaseArgs
	{

	public:
		//卡密(必填)
		string cardnum;
	};
}
#endif