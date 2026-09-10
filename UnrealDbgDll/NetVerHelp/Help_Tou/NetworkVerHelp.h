#pragma once
#ifndef NetworkVerHelp_H
#define NetworkVerHelp_H
//#define _CRT_SECURE_NO_WARNINGS
#include "../Help_Tou/HttpRequestTools.h"
#include <windows.h>
#include <ctime>
#include <string>
#include "../Model/EncryptType.h"
#include "../Model/BusinessType.h"
#include "../Model/PlatformType.h"
#include "../Model/DisableCardOrAccountType.h"
#include "../Model/Out_DataBaseArgs.h"
#include "../Model/Out_iniSoftInfo.h"
#include "../Model/Out_Login.h"
#include "../Model/Out_heartBeat.h"
#include "../Model/Out_getRemoteVar.h"
#include "../Model/Out_getCalculate.h"
#include "../Model/Out_unbundMacArgs.h"
#include "../Model/Out_OpenRenewAccount.h"
#include "../Model/Out_SearchOrder.h"
#include "../Model/Out_cardDetail.h"
#include "../Model/In_IniSoftInfoArgs.h"
#include "../Model/In_HeartBeatArgs.h"
#include "../Model/In_CardLoginArgs.h"
#include "../Model/In_LoginOutArgs.h"
#include "../Model/In_getRemoteVarArgs.h"
#include "../Model/In_getCalculateArgs.h"
#include "../Model/In_unbundMacArgs.h"
#include "../Model/In_SearchOrderArgs.h"
#include "../Model/In_CreateCardArgs.h"
#include "../Model/In_cardDetailArgs.h"
#include "../Model/In_DisableCardOrAccountArgs.h"
#include "../Model/ApiRequestArgs.h"
#include "../Model/Result.h"
#include "CJsonObject.hpp"
#include "Rc4.h"
#include "Md5.h"


#define USE_VMP        1   // 0 = 关闭，1 = 开启

#if  USE_VMP
	#include "../../../Common/VMProtect/VMProtectSDK.h"
	#ifdef _WIN64 
	#pragma comment(lib,"VMProtectSDK64.lib")   //VMProtectSDK64.lib   //vmprotectsdk64.lib
	#else
	#pragma comment(lib,"VMProtectSDK32.lib")
	#endif

#endif // ! 开启vmp



using namespace std;
namespace Help
{

	/// <summary>
	/// //软件初始化的结果对象
	/// </summary>
	extern  Model::Out_iniSoftInfo iniSoftInfoData;

	/// <summary>
	/// 获取机器码的实体类
	/// </summary>
	class NetworkVerHelp
	{
	public:

		/// <summary>
		/// //获取时间戳
		/// </summary>
		/// <returns></returns>
		static	__int64 GetTimeStamp();

		/// <summary>
		/// //获取机器码
		/// </summary>
		/// <returns></returns>
		static string GetMac();



		/// <summary>
		/// //json字符串转基础返回类型
		/// </summary>
		/// <param name="data"></param>
		/// <returns></returns>
		static Model::Out_DataBaseArgs ConvertOutDataBaseArgs(string data);

		//初始化软件返的jsonData数据转化成对象
		static Model::Out_iniSoftInfo ConvertIniSoftInfoData(string data);

		//登录的jsonData数据转化成对象
		static Model::Out_Login ConvertLoginData(string data);



		//将心跳数据转化成对象
		static Model::Out_heartBeat ConvertHeartBeat(string data);

		//换绑daga结果转化成对象
		static Model::Out_unbundMacArgs ConvertUnbundMac(string data);

		//卡密详情结果转对象
		static Model::Out_cardDetail ConvertCardDetail(string data);





		//初始化软件
		static Model::Result GetIniSoftInfo(Model::In_IniSoftInfoArgs dataArgs, Model::PlatformType _platformtypeid, string _encryptKey,
			string _signSalt, Model::EncryptType _encrypttypeid, string _softcode, string _platformUserCode);

		//登录成功后，开始心跳(心跳间隔时间不能小于5分钟)
		static Model::Result GetHeartBeat(Model::In_HeartBeatArgs dataArgs);



		/// <summary>
		/// 卡密登录
		/// </summary>
		/// <param name="dataArgs"></param>
		/// <returns></returns>
		static Model::Result GetLoginByCard(Model::In_CardLoginArgs dataArgs);

		/// <summary>
		/// //卡密详情
		/// </summary>
		/// <param name="dataArgs"></param>
		/// <returns></returns>
		static Model::Result GeCardDetail(Model::In_cardDetailArgs dataArgs);

		//退出登录
		static Model::Result GeLoginOut(Model::In_LoginOutArgs dataArgs);

		//解绑机器码(如果软件设置解绑扣除相应的时间或点数，那么解绑成功后会自动扣除)
		static Model::Result unbundMac(Model::In_unbundMacArgs dataArgs);

		//禁用还是删除：单码或账号
		static Model::Result DisableCardOrAccount(Model::In_DisableCardOrAccountArgs dataArgs);


	private:
		static Model::Result getReqResult(string address, string PostJsonData, CJsonObject oJson);
		//请求接口获取数据
		static Model::Result GetRequestResult(Model::BusinessType businessType, string data);

		static string GBKToUTF8(const char* strGBK);

		static	string UTF8ToGBK(const char* strUTF8);


	};

}
#endif