#pragma once
#ifndef In_CreateCardArgs_H_
#define In_CreateCardArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	// 生成卡密入参
	class In_CreateCardArgs :public In_DataBaseArgs
	{
	public:
		// 卡密前缀(1.只能填写数字或字母 2.长度不能大于4)，可选
		string prefix;
		// qq(可选)
		string qq;
		// wx(可选)
		string wx;
		// 支付宝(可选)
		string alipay;
		// 电话(可选)
		string tel;
		// 邮箱(可选)
		string email;
	};
}
#endif