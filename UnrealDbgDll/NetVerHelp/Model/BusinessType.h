#pragma once
#ifndef BusinessType_H_
#define BusinessType_H_
namespace Model
{
	//业务类型
	enum BusinessType
	{
		//软件初始化
		iniSoftInfo = 1,
		//账号注册
		accountRegister = 2,
		//账号登录
		accountLogin = 3,
		//卡密登录
		cardLogin = 4,
		//心跳
		heartBeat = 5,
		//扣点
		bucklePoint = 6,
		//退出登录
		loginOut = 7,
		//获取软件变量
		getremoteVar = 8,
		//解绑机器码
		unbundMac = 9,
		//修改用户密码
		updPwd = 10,

		//生成卡密
		createCard = 11,

		// 开通/续费卡密
		openRenewCardNum = 12,

		//开通/续费账号
		openRenewAccount = 13,

		//卡密详情
		cardDetail = 14,

		//账号详情
		accountDetail = 15,

		//获取软件价格列表
		softPriceList = 16,

		// 购买卡密
		buyCardNum = 17,

		//修改卡密/账号备注
		updRemark = 18,

		//充值卡充值
		rechCardRenewCardOrAccount = 19,

		//充值卡详情
		rechCardNumDetail = 20,

		//远程算法转发
		remoteCalculate = 21,

		//禁用还是删除：单码或账号
		disableCardOrAccount = 22,

	};
}
#endif