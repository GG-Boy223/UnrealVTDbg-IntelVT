#include "../Help_Tou/NetworkVerHelp.h"
#include "../../Crypter/xorstr.hpp"
#include <chrono>

namespace Help
{

	//瑞科网络验证：www.rukeyz.com

	static bool IsIniSoftInfo = false;

	//平台类型  1:网络验证平台  2:自动发货平台
	static Model::PlatformType platformtypeid;

	static Model::In_HeartBeatArgs dataArgsTemp;

	static string  endtimeTemp = (const char*)_xor_("");
	static int	surpluspointvalueTemp = 0;

	// 网络验证服务器地址
	static string ApiAddress = (const char*)_xor_("http://api.ruikeyz.com/NetVer/webapi");

	//订单查询地址
	static string SearchOrderAddress;

	// 加密的key
	// 注意：如果软件设置了加密通讯，那么此值必填
	static string encryptKey;

	// 签名盐(此参数是在软件列表里面进行设置)
	// 注意：如果软件设置了加密通讯，那么此值必填
	static string signSalt;

	//请求接口时，业务数据data参数加密码通讯类型(此参数是在软件列表里面进行设置)
	static Model::EncryptType encrypttypeid;

	//软件管理-->软件列表,里面找到相对应的goodscode,填写到此处
	static string goodscode;

	// 个人中心里面可以查看得到。代理商的话，那么在：代理管理--》代理商列表，可以查看得到
	// 注意：如果是作者自己就填写自己的platformUserCode,如果是代理商的，得填写代理商的Code
	static string platformUserCode;

	//初始化软件Key值(通过软件初始化接口【iniSoftInfo】获取，除了"软件初始化接口"不需要填写，其它接口必填写)
	static string inisoftkey;

	//软件初始化的结果对象
	Model::Out_iniSoftInfo iniSoftInfoData;

	//---------------------------------------------------------------------------------------------------

	string NetworkVerHelp::GBKToUTF8(const char* strGBK)
	{
#if USE_VMP
		VMProtectBegin("GBKToUTF8");
#endif
		int len = MultiByteToWideChar(CP_ACP, 0, strGBK, -1, NULL, 0);
		wchar_t* wstr = new wchar_t[len + 1];
		memset(wstr, 0, len + 1);
		MultiByteToWideChar(CP_ACP, 0, strGBK, -1, wstr, len);
		len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
		char* str = new char[len + 1];
		memset(str, 0, len + 1);
		WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
		string strTemp = str;
		if (wstr) delete[] wstr;
		if (str) delete[] str;

#if USE_VMP
		VMProtectEnd();
#endif

		return strTemp;
	}

	string NetworkVerHelp::UTF8ToGBK(const char* strUTF8)
	{
#if USE_VMP
		VMProtectBegin("UTF8ToGBK");
#endif
		int len = MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, NULL, 0);
		wchar_t* wszGBK = new wchar_t[len + 1];
		memset(wszGBK, 0, len * 2 + 2);
		MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, wszGBK, len);
		len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
		char* szGBK = new char[len + 1];
		memset(szGBK, 0, len + 1);
		WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, NULL, NULL);
		string strTemp(szGBK);
		if (wszGBK) delete[] wszGBK;
		if (szGBK) delete[] szGBK;

#if USE_VMP
		VMProtectEnd();
#endif

		return strTemp;

	}


	//获取时间戳
	__int64 NetworkVerHelp::GetTimeStamp()
	{
		const auto now = std::chrono::system_clock::now();
		const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
		return static_cast<__int64>(timestamp.count());
	}



    //请求接口获取数据
	Model::Result NetworkVerHelp::getReqResult(string address, string PostJsonData, CJsonObject oJson) {
#if USE_VMP
        VMProtectBegin("getReqResult");
#endif
		Model::Result result;
		result.code = -999;
		result.msg = (const char*)_xor_("服务器访问失败，您本地网络不顺畅，请检查下（或许您用了代理IP了，当时的代理IP不稳定？）");

		CHttpRequestTool http;
		list<string> head;
		head.push_back((const char*)_xor_("Accept: application/json, text/javascript, */*;"));
		head.push_back((const char*)_xor_("Content-Type: application/json;charset=UTF-8"));

		string ResponseResult = http.HttpPost(address, PostJsonData, head, 1000 * 30);
		if (oJson.Parse(ResponseResult))
		{
			oJson.Get((const char*)_xor_("code"), result.code);
			oJson.Get((const char*)_xor_("msg"), result.msg);
			oJson.Get((const char*)_xor_("data"), result.data);
		}
		head.clear();
		http.~CHttpRequestTool();

#if USE_VMP
		VMProtectEnd();
#endif

		return result;

	}

	//请求接口获取数据
	Model::Result NetworkVerHelp::GetRequestResult(Model::BusinessType businessType, string data)
	{
#if USE_VMP
        VMProtectBegin("GetRequestResult");
#endif
		Model::Result result;

		string sign = (const char*)_xor_("");//登录成功后，并且软件设置了加密码通讯，那么sign是必填项
		__int64 TimeStamp = GetTimeStamp();
		if (encrypttypeid != Model::EncryptType::NoEncrypt)
		{
			//如果软件设置了加密码通讯
			if (signSalt == (const char*)_xor_(""))
			{
				result.code = -999;
				result.msg = (const char*)_xor_("签名盐不能为空！");
				return result;
			}
			if (encryptKey == (const char*)_xor_(""))
			{
				result.code = -999;
				result.msg = (const char*)_xor_("加密key不能为空！");
				return result;
			}
		}

		switch (encrypttypeid)
		{
		case Model::NoEncrypt:
			break;
		case Model::RC4:
			string	UTF8data = GBKToUTF8(data.c_str());
			//如果软件设置是加密通讯，那么此处需要把data解密出来
			string	UTF8encryptKey = GBKToUTF8(encryptKey.c_str());
			Rc4 rc4;
			data = rc4.Encrypt(UTF8data.c_str(), UTF8encryptKey.c_str());

			//注意加密顺序不能变:业务类型+加密类型+平台用户Code+商品Code+inisoftkey+时间戳+业务数据+签名盐+平台类型
			string signData = to_string(businessType) + to_string(encrypttypeid) + platformUserCode + goodscode
				+ inisoftkey + to_string(TimeStamp) + data + signSalt + to_string(platformtypeid);
			signData = GBKToUTF8(signData.c_str());

			MD5 md5(signData);
			sign = md5.toString();
			break;
		}


		Model::ApiRequestArgs ApiArgs;
		ApiArgs.businessid = businessType;//必填
		ApiArgs.platformusercode = platformUserCode;//必填
		ApiArgs.encrypttypeid = encrypttypeid;//必填
		ApiArgs.goodscode = goodscode;//必填
		ApiArgs.timestamp = TimeStamp;//必填
		ApiArgs.data = data;//必填
		ApiArgs.sign = sign;//登录成功后，并且软件设置了加密码通讯，那么sign是必填项，否则可以为空
		ApiArgs.platformtypeid = platformtypeid;//必填


		CJsonObject oJson;
		oJson.Add((const char*)_xor_("businessid"), ApiArgs.businessid);
		oJson.Add((const char*)_xor_("platformusercode"), ApiArgs.platformusercode);
		oJson.Add((const char*)_xor_("encrypttypeid"), ApiArgs.encrypttypeid);
		oJson.Add((const char*)_xor_("goodscode"), ApiArgs.goodscode);
		oJson.Add((const char*)_xor_("inisoftkey"), inisoftkey);
		oJson.Add((const char*)_xor_("timestamp"), ApiArgs.timestamp);
		oJson.Add((const char*)_xor_("data"), ApiArgs.data);
		oJson.Add((const char*)_xor_("sign"), ApiArgs.sign);
		oJson.Add((const char*)_xor_("platformtypeid"), ApiArgs.platformtypeid);
		string PostJsonData = oJson.ToString();
		oJson.Clear();

		int ReqNum = 3;
		int repLogOutNum = 0;
		int SleepReqTime = 1;
		if (Model::BusinessType::remoteCalculate == businessType)
		{
			result = getReqResult((const char*)_xor_("http://api2.ruikeyz.com/NetVer/webapi"), PostJsonData, oJson);
		}
		else
		{
			for (size_t i = 0; i < ReqNum; i++)
			{
				result = getReqResult(ApiAddress, PostJsonData, oJson);
				if (result.code == -999)
				{
					result = getReqResult((const char*)_xor_("http://api2.ruikeyz.com/NetVer/webapi"), PostJsonData, oJson);
					if (businessType == Model::BusinessType::heartBeat)
					{
						if (result.code == 1012) {
							Model::Out_heartBeat out_HeartBeat1;
							out_HeartBeat1.heartbeatkey = dataArgsTemp.heartbeatkey;
							out_HeartBeat1.requestflag = dataArgsTemp.requestflag;
							out_HeartBeat1.endtime = endtimeTemp;
							out_HeartBeat1.surpluspointvalue = surpluspointvalueTemp;
							CJsonObject oJson2;
							oJson2.Add((const char*)_xor_("heartbeatkey"), out_HeartBeat1.heartbeatkey);
							oJson2.Add((const char*)_xor_("requestflag"), out_HeartBeat1.requestflag);
							oJson2.Add((const char*)_xor_("endtime"), out_HeartBeat1.endtime);
							oJson2.Add((const char*)_xor_("surpluspointvalue"), out_HeartBeat1.surpluspointvalue);
							string	data2 = oJson2.ToString();
							switch (encrypttypeid)
							{
							case Model::NoEncrypt:
								break;
							case Model::RC4:
								string	UTF8data2 = GBKToUTF8(data2.c_str());
								string	UTF8encryptKey2 = GBKToUTF8(encryptKey.c_str());
								Rc4 rc4;
								data2 = rc4.Encrypt(UTF8data2.c_str(), UTF8encryptKey2.c_str());
								break;
							}
							result.data = data2;
							result.code = 0;
							break;
						}
					}
				}
				if (businessType == Model::BusinessType::heartBeat)
				{
					if (result.code == -999 || result.code == 1001 || result.code == 1002) {
						Model::Out_heartBeat out_HeartBeat1;
						out_HeartBeat1.heartbeatkey = dataArgsTemp.heartbeatkey;
						out_HeartBeat1.requestflag = dataArgsTemp.requestflag;
						out_HeartBeat1.endtime = endtimeTemp;
						out_HeartBeat1.surpluspointvalue = surpluspointvalueTemp;
						CJsonObject oJson2;
						oJson2.Add((const char*)_xor_("heartbeatkey"), out_HeartBeat1.heartbeatkey);
						oJson2.Add((const char*)_xor_("requestflag"), out_HeartBeat1.requestflag);
						oJson2.Add((const char*)_xor_("endtime"), out_HeartBeat1.endtime);
						oJson2.Add((const char*)_xor_("surpluspointvalue"), out_HeartBeat1.surpluspointvalue);
						string	data2 = oJson2.ToString();
						switch (encrypttypeid)
						{
						case Model::NoEncrypt:
							break;
						case Model::RC4:
							string	UTF8data2 = GBKToUTF8(data2.c_str());
							string	UTF8encryptKey2 = GBKToUTF8(encryptKey.c_str());
							Rc4 rc4;
							data2 = rc4.Encrypt(UTF8data2.c_str(), UTF8encryptKey2.c_str());
							break;
						}
						result.data = data2;
						result.code = 0;
						break;
					}
				}
				if (result.code == -999)
				{
					result = getReqResult((const char*)_xor_("http://api3.ruikeyz.com/NetVer/webapi"), PostJsonData, oJson);
				}
				if (result.code != -999)
				{
					break;
				}
				Sleep(1000 * SleepReqTime);
			}
		}
		

#if USE_VMP
		VMProtectEnd();
#endif
		return result;
	}

	//json字符串转基础返回类型
	Model::Out_DataBaseArgs NetworkVerHelp::ConvertOutDataBaseArgs(string data)
	{
#if USE_VMP
		VMProtectBegin("ConvertOutDataBaseArgs");
#endif
		Model::Out_DataBaseArgs result;
		if (data == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("data不能为空！"));
		}
		if (encrypttypeid != Model::EncryptType::NoEncrypt)
		{
			if (encryptKey == (const char*)_xor_(""))
			{
				throw exception((const char*)_xor_("加密key不能为空！"));
			}

			switch (encrypttypeid)
			{
			case Model::NoEncrypt:
				break;
			case Model::RC4:
				//如果软件设置是加密通讯，那么此处需要把data解密出来
				string	UTF8encryptKey = GBKToUTF8(encryptKey.c_str());
				Rc4 rc4;
				data = rc4.Decrypt(data.c_str(), UTF8encryptKey.c_str());
				data = UTF8ToGBK(data.c_str());
				break;
			}

		}


		CJsonObject oJson(data);
		oJson.Get((const char*)_xor_("requestflag"), result.requestflag);
		oJson.Get((const char*)_xor_("servertimestamp"), result.servertimestamp);

#if USE_VMP
		VMProtectEnd();
#endif

		return result;

	}

	//初始化软件业务数据结果转对象
	Model::Out_iniSoftInfo NetworkVerHelp::ConvertIniSoftInfoData(string data)
	{
#if USE_VMP
		VMProtectBegin("ConvertIniSoftInfoData");
#endif
		Model::Out_iniSoftInfo result;
		if (data == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("data不能为空！"));
		}
		if (encrypttypeid != Model::EncryptType::NoEncrypt)
		{
			if (encryptKey == (const char*)_xor_(""))
			{
				throw exception((const char*)_xor_("加密key不能为空！"));
			}

			switch (encrypttypeid)
			{
			case Model::NoEncrypt:
				break;
			case Model::RC4:
				//如果软件设置是加密通讯，那么此处需要把data解密出来
				string	UTF8encryptKey = GBKToUTF8(encryptKey.c_str());
				Rc4 rc4;
				data = rc4.Decrypt(data.c_str(), UTF8encryptKey.c_str());
				data = UTF8ToGBK(data.c_str());
				break;
			}

		}


		CJsonObject oJson(data);
		oJson.Get((const char*)_xor_("inisoftkey"), result.inisoftkey);
		oJson.Get((const char*)_xor_("requestflag"), result.requestflag);
		oJson.Get((const char*)_xor_("servertimestamp"), result.servertimestamp);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("softname"), result.softInfo.softname);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("consultwebsite"), result.softInfo.consultwebsite);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("consultqq"), result.softInfo.consultqq);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("consultwx"), result.softInfo.consultwx);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("consulttel"), result.softInfo.consulttel);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("logourl"), result.softInfo.logourl);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("notice"), result.softInfo.notice);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("opentestcount"), result.softInfo.opentestcount);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("opentestday"), result.softInfo.opentestday);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("basedata"), result.softInfo.basedata);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("newversionnum"), result.softInfo.newversionnum);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("networkdiskurl"), result.softInfo.networkdiskurl);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("diskpwd"), result.softInfo.diskpwd);
		oJson[(const char*)_xor_("softInfo")].Get((const char*)_xor_("isforceupd"), result.softInfo.isforceupd);

#if USE_VMP
		VMProtectEnd();
#endif

		return result;

	}

	//登录业务结果转转对象
	Model::Out_Login NetworkVerHelp::ConvertLoginData(string data)
	{
#if USE_VMP
		VMProtectBegin("ConvertLoginData");
#endif
		Model::Out_Login result;
		if (data == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("data不能为空！"));
		}
		if (encrypttypeid != Model::EncryptType::NoEncrypt)
		{
			if (encryptKey == (const char*)_xor_(""))
			{
				throw exception((const char*)_xor_("加密key不能为空！"));
			}

			switch (encrypttypeid)
			{
			case Model::NoEncrypt:
				break;
			case Model::RC4:
				//如果软件设置是加密通讯，那么此处需要把data解密出来
				string	UTF8encryptKey = GBKToUTF8(encryptKey.c_str());
				Rc4 rc4;
				data = rc4.Decrypt(data.c_str(), UTF8encryptKey.c_str());
				data = UTF8ToGBK(data.c_str());
				break;
			}

		}

		CJsonObject oJson(data);
		int ispay = 0;
		oJson.Get((const char*)_xor_("requestflag"), result.requestflag);
		oJson.Get((const char*)_xor_("servertimestamp"), result.servertimestamp);
		oJson.Get((const char*)_xor_("token"), result.token);
		oJson.Get((const char*)_xor_("heartbeatkey"), result.heartbeatkey);
		oJson.Get((const char*)_xor_("endtime"), result.endtime);
		oJson.Get((const char*)_xor_("surpluspointvalue"), result.surpluspointvalue);
		oJson.Get((const char*)_xor_("currentloginnum"), result.currentloginnum);
		oJson.Get((const char*)_xor_("priceid"), result.priceid);
		oJson.Get((const char*)_xor_("pricename"), result.pricename);
		oJson.Get((const char*)_xor_("ispay"), ispay);
		oJson.Get((const char*)_xor_("roleid"), result.roleid);
		oJson.Get((const char*)_xor_("rolename"), result.rolename);
		oJson.Get((const char*)_xor_("maxloginnum"), result.maxloginnum);
		oJson.Get((const char*)_xor_("qq"), result.qq);
		oJson.Get((const char*)_xor_("wx"), result.wx);
		oJson.Get((const char*)_xor_("alipay"), result.alipay);
		oJson.Get((const char*)_xor_("tel"), result.tel);
		oJson.Get((const char*)_xor_("email"), result.email);
		oJson.Get((const char*)_xor_("remarks"), result.remarks);
		/*result.ispay = (Model::IsPay)ispay;*/


		endtimeTemp = result.endtime;
		surpluspointvalueTemp = result.surpluspointvalue;


#if USE_VMP
		VMProtectEnd();
#endif

		return result;

	}

	//心跳业务结果转对象
	Model::Out_heartBeat NetworkVerHelp::ConvertHeartBeat(string data)
	{
#if USE_VMP
		VMProtectBegin("ConvertHeartBeat");
#endif
		Model::Out_heartBeat result;
		if (data == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("data不能为空！"));
		}
		if (encrypttypeid != Model::EncryptType::NoEncrypt)
		{
			if (encryptKey == (const char*)_xor_(""))
			{
				throw exception((const char*)_xor_("加密key不能为空！"));
			}

			switch (encrypttypeid)
			{
			case Model::NoEncrypt:
				break;
			case Model::RC4:
				//如果软件设置是加密通讯，那么此处需要把data解密出来
				string	UTF8encryptKey = GBKToUTF8(encryptKey.c_str());
				Rc4 rc4;
				data = rc4.Decrypt(data.c_str(), UTF8encryptKey.c_str());
				data = UTF8ToGBK(data.c_str());
				break;
			}

		}
		CJsonObject oJson(data);
		oJson.Get((const char*)_xor_("requestflag"), result.requestflag);
		oJson.Get((const char*)_xor_("servertimestamp"), result.servertimestamp);
		oJson.Get((const char*)_xor_("heartbeatkey"), result.heartbeatkey);
		oJson.Get((const char*)_xor_("endtime"), result.endtime);
		oJson.Get((const char*)_xor_("surpluspointvalue"), result.surpluspointvalue);

#if USE_VMP
		VMProtectEnd();
#endif


		return result;

	}



	//卡密详情结果转对象
	Model::Out_cardDetail NetworkVerHelp::ConvertCardDetail(string data)
	{
#if USE_VMP
		VMProtectBegin("ConvertCardDetail");
#endif
		Model::Out_cardDetail result;
		if (data == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("data不能为空！"));
		}
		if (encrypttypeid != Model::EncryptType::NoEncrypt)
		{
			if (encryptKey == (const char*)_xor_(""))
			{
				throw exception((const char*)_xor_("加密key不能为空！"));
			}

			switch (encrypttypeid)
			{
			case Model::NoEncrypt:
				break;
			case Model::RC4:
				//如果软件设置是加密通讯，那么此处需要把data解密出来
				string	UTF8encryptKey = GBKToUTF8(encryptKey.c_str());
				Rc4 rc4;
				data = rc4.Decrypt(data.c_str(), UTF8encryptKey.c_str());
				data = UTF8ToGBK(data.c_str());
				break;
			}

		}
		CJsonObject oJson(data);
		int ispay = 0;
		int openstate = 0;
		int activestate = 0;
		oJson.Get((const char*)_xor_("requestflag"), result.requestflag);
		oJson.Get((const char*)_xor_("servertimestamp"), result.servertimestamp);
		oJson.Get((const char*)_xor_("endtime"), result.endtime);
		oJson.Get((const char*)_xor_("surpluspointvalue"), result.surpluspointvalue);
		oJson.Get((const char*)_xor_("maccode"), result.maccode);
		oJson.Get((const char*)_xor_("priceid"), result.priceid);
		oJson.Get((const char*)_xor_("pricename"), result.pricename);
		oJson.Get((const char*)_xor_("ispay"), ispay);
		oJson.Get((const char*)_xor_("qq"), result.qq);
		oJson.Get((const char*)_xor_("wx"), result.wx);
		oJson.Get((const char*)_xor_("alipay"), result.alipay);
		oJson.Get((const char*)_xor_("tel"), result.tel);
		oJson.Get((const char*)_xor_("email"), result.email);
		oJson.Get((const char*)_xor_("regtime"), result.regtime);
		oJson.Get((const char*)_xor_("openstate"), openstate);
		oJson.Get((const char*)_xor_("activestate"), activestate);
		oJson.Get((const char*)_xor_("remarks"), result.remarks);
		/*result.ispay = (Model::IsPay)ispay;*/
		// /*result.ispay = (Model::IsPay)ispay;*/
		result.openstate = (Model::OpenState)openstate;
		result.activestate = (Model::ActiveState)activestate;

#if USE_VMP
		VMProtectEnd();
#endif

		return result;

	}

	//初始化软件
	Model::Result NetworkVerHelp::GetIniSoftInfo(Model::In_IniSoftInfoArgs dataArgs, Model::PlatformType _platformtypeid, string _encryptKey,
		string _signSalt, Model::EncryptType _encrypttypeid, string _goodscode, string _platformUserCode)
	{
#if USE_VMP
		VMProtectBegin("GetIniSoftInfo");
#endif
		Model::Result result;
		if (IsIniSoftInfo == false)
		{
			//NetworkVerHelp::ini();

			platformtypeid = _platformtypeid;
			encryptKey = _encryptKey;
			signSalt = _signSalt;
			encrypttypeid = _encrypttypeid;
			goodscode = _goodscode;
			platformUserCode = _platformUserCode;

			size_t length = ApiAddress.find_last_of((const char*)_xor_("/"));
			if (length + 1 == ApiAddress.size())
			{
				ApiAddress = ApiAddress.substr(0, length - 1);
			}
			SearchOrderAddress = ApiAddress.substr(0, ApiAddress.find_last_of((const char*)_xor_("/"))) + (const char*)_xor_("/SearchOrder");


			string data = (const char*)_xor_("");
			CJsonObject oJson;
			oJson.Add((const char*)_xor_("requestflag"), dataArgs.requestflag);
			oJson.Add((const char*)_xor_("versionname"), dataArgs.versionname);
			oJson.Add((const char*)_xor_("maccode"), dataArgs.maccode);
			oJson.Add((const char*)_xor_("timestamp"), dataArgs.timestamp);
			data = oJson.ToString();

			result = GetRequestResult(Model::BusinessType::iniSoftInfo, data);
			if (result.code == 0)
			{
				switch (platformtypeid)
				{
				case Model::NetVer:
					iniSoftInfoData = ConvertIniSoftInfoData(result.data);
					inisoftkey = iniSoftInfoData.inisoftkey;//初始化完软件后，把此Key保留下来，其它接口此Key必填写的
					break;
				}

			}


		}
		else
		{
			result.msg = (const char*)_xor_("软件已初始化过了，不能再次进行初始化");
		}

#if USE_VMP
		VMProtectEnd();
#endif
		return result;

	}

	//登录成功后，开始心跳(心跳间隔时间不能小于5分钟)
	Model::Result NetworkVerHelp::GetHeartBeat(Model::In_HeartBeatArgs dataArgs)
	{
#if USE_VMP
		VMProtectBegin("GetHeartBeat");
#endif

		Model::Result result;
		if (dataArgs.token == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("token登录令牌不能为空！"));
		}
		string data = (const char*)_xor_("");
		CJsonObject oJson;
		oJson.Add((const char*)_xor_("requestflag"), dataArgs.requestflag);
		oJson.Add((const char*)_xor_("maccode"), dataArgs.maccode);
		oJson.Add((const char*)_xor_("timestamp"), dataArgs.timestamp);
		oJson.Add((const char*)_xor_("cardnumorusername"), dataArgs.cardnumorusername);
		oJson.Add((const char*)_xor_("token"), dataArgs.token);
		oJson.Add((const char*)_xor_("heartbeatkey"), dataArgs.heartbeatkey);
		data = oJson.ToString();

		dataArgsTemp = dataArgs;
		result = GetRequestResult(Model::BusinessType::heartBeat, data);

#if USE_VMP
		VMProtectEnd();
#endif
		return result;
	}

	//卡密登录
	Model::Result NetworkVerHelp::GetLoginByCard(Model::In_CardLoginArgs dataArgs)
	{
#if USE_VMP
		VMProtectBegin("GetLoginByCard");
#endif
		Model::Result result;
		string data = (const char*)_xor_("");
		CJsonObject oJson;
		oJson.Add((const char*)_xor_("requestflag"), dataArgs.requestflag);
		oJson.Add((const char*)_xor_("maccode"), dataArgs.maccode);
		oJson.Add((const char*)_xor_("timestamp"), dataArgs.timestamp);
		oJson.Add((const char*)_xor_("cardnum"), dataArgs.cardnum);
		data = oJson.ToString();
		result = GetRequestResult(Model::BusinessType::cardLogin, data);


#if USE_VMP
		VMProtectEnd();
#endif

		return result;
	}

	//卡密详情
	Model::Result NetworkVerHelp::GeCardDetail(Model::In_cardDetailArgs dataArgs)
	{
#if USE_VMP
		VMProtectBegin("GeCardDetail");
#endif
		Model::Result result;
		string data = (const char*)_xor_("");
		CJsonObject oJson;
		oJson.Add((const char*)_xor_("requestflag"), dataArgs.requestflag);
		oJson.Add((const char*)_xor_("maccode"), dataArgs.maccode);
		oJson.Add((const char*)_xor_("timestamp"), dataArgs.timestamp);
		oJson.Add((const char*)_xor_("cardnum"), dataArgs.cardnum);
		data = oJson.ToString();
		result = GetRequestResult(Model::BusinessType::cardDetail, data);

#if USE_VMP
		VMProtectEnd();
#endif

		return result;
	}


	//退出登录
	Model::Result NetworkVerHelp::GeLoginOut(Model::In_LoginOutArgs dataArgs)
	{
#if USE_VMP
		VMProtectBegin("GeLoginOut");
#endif
		Model::Result result;
		string data = (const char*)_xor_("");
		if (dataArgs.token == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("token登录令牌不能为空!"));
		}
		CJsonObject oJson;
		oJson.Add((const char*)_xor_("maccode"), dataArgs.maccode);
		oJson.Add((const char*)_xor_("timestamp"), dataArgs.timestamp);
		oJson.Add((const char*)_xor_("cardnumorusername"), dataArgs.cardnumorusername);
		oJson.Add((const char*)_xor_("token"), dataArgs.token);
		data = oJson.ToString();
		result = GetRequestResult(Model::BusinessType::loginOut, data);


#if USE_VMP
		VMProtectEnd();
#endif
		return result;

	}

	//禁用还是删除：单码或账号
	Model::Result NetworkVerHelp::DisableCardOrAccount(Model::In_DisableCardOrAccountArgs dataArgs)
	{
#if USE_VMP
		VMProtectBegin("DisableCardOrAccount");
#endif
		Model::Result result;
		string data = (const char*)_xor_("");
		if (dataArgs.token == (const char*)_xor_(""))
		{
			throw exception((const char*)_xor_("token登录令牌不能为空！"));
		}
		CJsonObject oJson;
		oJson.Add((const char*)_xor_("maccode"), dataArgs.maccode);
		oJson.Add((const char*)_xor_("timestamp"), dataArgs.timestamp);
		oJson.Add((const char*)_xor_("cardnumorusername"), dataArgs.cardnumorusername);
		oJson.Add((const char*)_xor_("token"), dataArgs.token);
		oJson.Add((const char*)_xor_("disablecardoraccounttype"), dataArgs.disablecardoraccounttype);
		data = oJson.ToString();
		result = GetRequestResult(Model::BusinessType::disableCardOrAccount, data);

#if USE_VMP
		VMProtectEnd();
#endif

		return result;

	}

}