#pragma once
#ifndef Out_Login_H_
#define Out_Login_H_
#include <string>
#include <list>
#include "Out_DataBaseArgs.h"
using namespace std;
namespace Model
{
	//登录出参
	class Out_Login :public Out_DataBaseArgs
	{

	public:
		//登录成功后的令牌
		string token;

		//心跳Key,每次心跳此值都会变
		string heartbeatkey;

		//卡密或账号到期时间
		string endtime;

		//卡密或账号剩余点数
		int surpluspointvalue;

		//此卡密或账号已登录的数量
		int currentloginnum;

		//开通的价格类型ID
		string priceid;

		//开通的价格类型名称
		string pricename;

		////是否已付款  0:未付款  1:已付款
		//IsPay ispay;

		//角色ID
		string roleid;

		//角色名称
		string rolename;

		//同一卡密或账号最大登录数量
		int maxloginnum;

		//账号用户的qq
		string qq;

		// 账号用户的微信
		string wx;

		// 账号用户的支付宝
		string alipay;

		// 账号用户的手机号
		string tel;

		// 账号用户的邮箱
		string email;

		//备注
		string remarks;

	};
}
#endif