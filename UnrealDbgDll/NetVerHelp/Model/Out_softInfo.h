#pragma once
#ifndef Out_softInfo_H_
#define Out_softInfo_H_
#include <string>
using namespace std;
namespace Model
{
	//软件信息
	class  Out_softInfo
	{
	public:
		//软件名称
		string softname;
		//软件咨询官网
		string consultwebsite;

		//软件咨询qq
		string consultqq;

		//软件咨询微信
		string consultwx;

		//软件咨询电话
		string consulttel;
		// 软件logo下载地址
		string logourl;
		//开通测试卡密或账号：N天内只能开通测试号的总数量
		int opentestcount;
		//开通测试卡密或账号：天数
		int opentestday;
		//公告
		string notice;
		//软件基础数据
		string basedata;
		//最新版本号
		string newversionnum;
		//软件下载的网盘地址
		string networkdiskurl;
		//网盘提取码
		string diskpwd;
		//是否强制更新  0：不强制 1:强制
		int isforceupd;
	};
}
#endif