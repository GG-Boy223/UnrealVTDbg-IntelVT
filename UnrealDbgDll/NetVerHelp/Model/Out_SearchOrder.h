#pragma once
#ifndef Out_SearchOrder_H_
#define Out_SearchOrder_H_
#include <string>

#include "Out_DataBaseArgs.h"
using std::string;
using namespace std;
namespace Model
{
	//订单查询出参
	class Out_SearchOrder :public Out_DataBaseArgs
	{

	public:





		// 售价
		double price;

		// 实收金额
		double revicemoney;

		// 卡密 
		// 【注意：如果此订单号属于“网络验证平台”产生的，并且是购买卡密，那么支付状态和订单业务完成状态都成功后，会把购买的卡密返回来】
		string cardnum;

		// 商品下载的网盘地址
		// 【如果此订单号属于“自动发货平台”产生的，那么支付状态和订单业务完成状态都成功后，会把商品的下载地址和提取码返回来】
		string goodsnetworkdiskurl;
		// 网盘提取码
		string diskpwd;

		//机器码【如果此订单号属于“自动发货平台”产生的，那么此订单在哪台设备上生成的机器码会返回来】
		string maccode;

		//订单标识，此订单是否发货的一个标记
		//【如果此订单号属于“自动发货平台”产生的，那么此订单标识会返回来】
		string orderflag;

		//订单创建的时间
		string createtime;
			 
	};
}
#endif