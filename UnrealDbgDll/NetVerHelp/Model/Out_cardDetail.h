#pragma once
#ifndef Out_cardDetail_H_
#define Out_cardDetail_H_
#include <string>
#include "OpenState.h"
#include "ActiveState.h"
#include "Out_DataBaseArgs.h"
using namespace std;
namespace Model
{
	/*卡密详情输出*/
	// /*卡密详情输出*/
	class Out_cardDetail :public Out_DataBaseArgs
	{
	public:
		/*
		*卡密到期时间
		*注意：只有卡密激活的情况下，服务器才会返回
		*/
		string endtime;

		/*
		*卡密剩余点数
		*注意：只有卡密激活的情况下，服务器才会返回
		*/
		// /*
		// *卡密剩余点数
		// *注意：只有卡密激活的情况下，服务器才会返回
		// */
		int surpluspointvalue;

		/*最近一次登录的机器码*/
		// /*最近一次登录的机器码*/
		string maccode;

		//开通的价格类型ID
		string priceid;

		//开通的价格类型名称
		string pricename;

		////是否已付款  0:未付款  1:已付款
		//IsPay ispay;

		//终端用户的qq
		string qq;

		//终端用户的微信
		string wx;

		//终端用户的支付宝
		string alipay;

		//终端用户的手机号
		string tel;

		//终端用户的邮箱
		string email;

		//生成时间
		string regtime;

		//备注
		string remarks;

		//开通状态  0:未开通  1:已开通
		OpenState openstate;

		//激活状态 0:未激活  1:已激活
		ActiveState activestate;
	};
}
#endif