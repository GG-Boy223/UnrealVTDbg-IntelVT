#pragma once
#ifndef ApiRequestArgs_H_
#define ApiRequestArgs_H_
#include <string>
#include "BusinessType.h"
#include "EncryptType.h"
#include "PlatformType.h"
using namespace std;
namespace Model
{
	//请求接口的入参
	class  ApiRequestArgs
	{
	public:
		//业务ID(必填)
		BusinessType businessid;
		//加密方式(必填)
		EncryptType encrypttypeid;
		// 平台用户Code(必填)
		// 说明：个人中心里面可以查看得到。代理商的话，那么在：代理管理--》代理商列表，可以查看得到
		string platformusercode;
		//商品Code(必填)
		string goodscode;
		//初始化软件Key值(通过软件初始化接口【iniSoftInfo】获取，除了"软件初始化接口"不需要填写，其它接口必填写)
		string inisoftkey;
		//时间戳(必填)
		__int64 timestamp;
		//请求的数据类Json字符串(具体参照API文档)
		string data;
		//签名
		//注意：
		//1.如果软件设置的是“无加密通讯”，那么此值可以为空，如果设置的是“加密通讯”，那么此值必填写
		//2.签名算法：md5(businessID+encrypttypeid+platformusercode+goodscode+inisoftkey+timestamp+data+signSalt+platformtypeid)
		string sign;

		//平台类型(必填) 1:网络验证平台  2:自动发货平台
		PlatformType platformtypeid;
	};

}
#endif