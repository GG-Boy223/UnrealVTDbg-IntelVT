#include "DllMainInternal.h"

Logger logger("UnrealDbgDll.log");

#ifdef 开启验证
string cardnum;
string heartbeatkey;
string token;
std::string maccode;
std::wstring macAdapterType;
std::wstring macAdapterName;

Model::PlatformType platformtypeid = Model::PlatformType::NetVer;
string encryptKey = (const char*)_xor_("29d6b27e");
string signSalt = (const char*)_xor_("5cae28fc");
Model::EncryptType encrypttypeid = Model::EncryptType::RC4;
string goodscode = (const char*)_xor_("4cd990453596e1aa");
string platformUserCode = (const char*)_xor_("86ffdabd03591bff4e958201c252fe75f0d1e74d");
string versionname = (const char*)_xor_("v6050");
#endif
