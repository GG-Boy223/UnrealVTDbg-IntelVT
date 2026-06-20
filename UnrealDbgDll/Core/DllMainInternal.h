#pragma once

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <iptypes.h>
#include <thread>

#include "../dllmain.h"
#include "../Init/InitNTDevice.h"
#include "../Channels/DispatchData.h"
#include "../Globals.h"
#include "../Interface/Interface.h"
#include "../StartProcess.h"
#include "../NetVerHelp/Help_Tou/NetworkVerHelp.h"
#include "../Crypter/xorstr.hpp"

#define 开启验证

#ifdef 开启验证
extern string cardnum;
extern string heartbeatkey;
extern string token;
extern std::string maccode;
extern std::wstring macAdapterType;
extern std::wstring macAdapterName;

extern Model::PlatformType platformtypeid;
extern string encryptKey;
extern string signSalt;
extern Model::EncryptType encrypttypeid;
extern string goodscode;
extern string platformUserCode;
extern string versionname;

std::string GetMac();
bool EnsureMacCode();
std::string FormatMacAddress(const BYTE* address, ULONG length);
std::wstring FormatMacAddressForLog(const std::string& mac);
void LogMultilineLocalText(
    const std::string& text,
    const wchar_t* firstLinePrefix = L"",
    const wchar_t* nextLinePrefix = L"");

void LoginOut(string cardnumorusername);
void DisableCardOrAccount(string cardnumorusername);
bool IniSoftInfo();
void HeartBeat();
bool LoginByCard(string _CardNum);
#endif

void InitSymbol();
void DispatchSymbol();
