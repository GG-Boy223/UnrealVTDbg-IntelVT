#include "DllMainInternal.h"

#ifdef 开启验证
void LoginOut(string cardnumorusername)
{
#ifdef USE_VMP
    VMProtectBegin("LoginOut");
#endif
    Model::In_LoginOutArgs LoginOutArgs;
    LoginOutArgs.maccode = maccode;
    LoginOutArgs.timestamp = Help::NetworkVerHelp::GetTimeStamp();
    LoginOutArgs.cardnumorusername = cardnumorusername;
    LoginOutArgs.token = token;
    Model::Result result = Help::NetworkVerHelp::GeLoginOut(LoginOutArgs);
#ifdef USE_VMP
    VMProtectEnd();
#endif
}

void DisableCardOrAccount(string cardnumorusername)
{
#ifdef USE_VMP
    VMProtectBegin("DisableCardOrAccount");
#endif

    Model::In_DisableCardOrAccountArgs DisableCardOrAccountArgs;
    DisableCardOrAccountArgs.maccode = maccode;
    DisableCardOrAccountArgs.timestamp = Help::NetworkVerHelp::GetTimeStamp();
    DisableCardOrAccountArgs.cardnumorusername = cardnumorusername;
    DisableCardOrAccountArgs.token = token;
    DisableCardOrAccountArgs.disablecardoraccounttype = Model::DisableCardOrAccountType::Disable;
    Model::Result result = Help::NetworkVerHelp::DisableCardOrAccount(DisableCardOrAccountArgs);
#ifdef USE_VMP
    VMProtectEnd();
#endif
}

bool IniSoftInfo()
{
#ifdef USE_VMP
    VMProtectBegin("IniSoftInfo");
#endif
    bool IniResult = false;

    Model::In_IniSoftInfoArgs args;
    args.maccode = maccode;
    args.timestamp = Help::NetworkVerHelp::GetTimeStamp();
    args.requestflag = to_string(args.timestamp);
    args.versionname = versionname;
    Model::Result result = Help::NetworkVerHelp::GetIniSoftInfo(args, platformtypeid, encryptKey, signSalt, encrypttypeid, goodscode, platformUserCode);

    if (Help::iniSoftInfoData.softInfo.newversionnum != (const char*)_xor_(""))
    {
        if (Help::iniSoftInfoData.softInfo.newversionnum != versionname)
        {
            logger.outDebug(_xor_(L"1.新版本下载地址:%s"), Common::ConvertLocalCodePageToWideString(Help::iniSoftInfoData.softInfo.networkdiskurl).c_str());
            logger.outDebug(_xor_(L"           密码：%s"), Common::ConvertLocalCodePageToWideString(Help::iniSoftInfoData.softInfo.diskpwd).c_str());

            IniResult = false;
#ifdef USE_VMP
            VMProtectEnd();
#endif
            return IniResult;
        }
    }

    if (result.code == 0)
    {
        if (Help::iniSoftInfoData.inisoftkey != (const char*)_xor_(""))
        {
            if (Help::iniSoftInfoData.requestflag != args.requestflag)
            {
                IniResult = false;
                DisableCardOrAccount(cardnum);
                exit(0);
                return IniResult;
            }
        }
        LogMultilineLocalText(Help::iniSoftInfoData.softInfo.notice);
        logger.outDebug(_xor_(L"----------------------------------------"));
        LogMultilineLocalText(Help::iniSoftInfoData.softInfo.basedata, _xor_(L"消息："), _xor_(L"      "));

        IniResult = true;
#ifdef USE_VMP
        VMProtectEnd();
#endif
        return IniResult;
    }

    switch (result.code)
    {
    case 1003:
        logger.outDebug(_xor_(L"通信异常"));
        break;
    case 1006:
        logger.outDebug(_xor_(L"机器码_时间戳_错误"));
        break;
    default:
        LogMultilineLocalText(result.msg);
        break;
    }

    IniResult = false;
#ifdef USE_VMP
    VMProtectEnd();
#endif
    return IniResult;
}

void HeartBeat()
{
    while (true)
    {
#ifdef USE_VMP
        VMProtectBegin("HeartBeatM");
#endif
        Model::In_HeartBeatArgs HeartBeatArgs;
        HeartBeatArgs.maccode = maccode;
        HeartBeatArgs.timestamp = Help::NetworkVerHelp::GetTimeStamp();
        HeartBeatArgs.requestflag = to_string(HeartBeatArgs.timestamp);
        HeartBeatArgs.cardnumorusername = cardnum;
        HeartBeatArgs.token = token;
        HeartBeatArgs.heartbeatkey = heartbeatkey;

        Model::Result result = Help::NetworkVerHelp::GetHeartBeat(HeartBeatArgs);
        if (result.code == 0)
        {
            Model::Out_heartBeat heartBeatResult = Help::NetworkVerHelp::ConvertHeartBeat(result.data);
            if (heartBeatResult.heartbeatkey != (const char*)_xor_("") && heartBeatResult.requestflag == HeartBeatArgs.requestflag)
            {
                heartbeatkey = heartBeatResult.heartbeatkey;
            }
            else
            {
                DisableCardOrAccount(cardnum);
                VMProtectEnd();
                exit(0);
            }
        }
        else
        {
            Sleep(1000);
            LoginOut(cardnum);
            VMProtectEnd();
            exit(0);
        }
        Sleep(300 * 1000);
#ifdef USE_VMP
        VMProtectEnd();
#endif
    }
}

bool LoginByCard(string _CardNum)
{
#ifdef USE_VMP
    VMProtectBegin("LoginByCardM");
#endif
    bool IsLoginOk = false;

    Model::In_CardLoginArgs args;
    args.maccode = maccode;
    args.timestamp = Help::NetworkVerHelp::GetTimeStamp();
    args.requestflag = to_string(args.timestamp);
    args.cardnum = _CardNum;
    Model::Result result = Help::NetworkVerHelp::GetLoginByCard(args);
    Sleep(1000);

    if (result.code == 0)
    {
        Model::Out_Login LoginData = Help::NetworkVerHelp::ConvertLoginData(result.data);

        if (LoginData.requestflag != args.requestflag)
        {
            IsLoginOk = false;

            DisableCardOrAccount(_CardNum);
            LoginOut(_CardNum);
            exit(0);
            return IsLoginOk;
        }

        token = LoginData.token;
        heartbeatkey = LoginData.heartbeatkey;
        IsLoginOk = true;
    }
    else
    {
        switch (result.code)
        {
        case 6003:
        case 6005:
            logger.outDebug(_xor_(L"温馨提示：当前时间不开放，请等待开放时间"));
            break;
        case 6002:
            logger.outDebug(_xor_(L"温馨提示：停用软件"));
            break;
        default:
            LogMultilineLocalText(result.msg, _xor_(L"温馨提示："), _xor_(L"          "));
            break;
        }
        IsLoginOk = false;
    }

#ifdef USE_VMP
    VMProtectEnd();
#endif

    return IsLoginOk;
}
#endif
