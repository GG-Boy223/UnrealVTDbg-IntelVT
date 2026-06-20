#pragma once
#ifndef In_SearchOrderArgs_H_
#define In_SearchOrderArgs_H_
#include <string>
#include "EncryptType.h"
#include "PlatformType.h"
using namespace std;
namespace Model
{
	//请求接口的入参
	class  In_SearchOrderArgs
	{
	public:
		// 平台用户Code(必填)
		// 说明：个人中心里面可以查看得到。代理商的话，那么在：代理管理--》代理商列表，可以查看得到
		string platformusercode;

		//商品Code(必填)
		string goodscode;

		/*
		请求标识，长度不能大于16个字符(必填)
		注意：接口返回数据的时候，会原本原样的把此值返回来。如果返回来的值与当前提交的值不一致，
		说明接口返回的数据有可能被“破解者”截持，那么您的软件可以直接关闭或不做任何处理
		*/
		string requestflag;

		//加密方式(必填)
		EncryptType encrypttypeid;

		//初始化软件Key值(通过软件初始化接口【iniSoftInfo】获取，除了"软件初始化接口"不需要填写，其它接口必填写)
		string inisoftkey;

		//订单号
		string orderid;

		//平台类型(必填) 1:网络验证平台  2:自动发货平台
		PlatformType platformtypeid;
	};

}
#endif