#pragma once
#ifndef EncryptType_H_
#define EncryptType_H_
namespace Model
{
	//加密类型
	enum EncryptType
	{
		// 无加密
		NoEncrypt = 0,
		//// DES加密(c#语言，建议用此加密方式)
		//DES = 1,
		///// <summary>
		///// AES加密
		///// </summary>
		//AES = 2,
		// RC4加密(c++语言，建议用此加密方式)
		RC4 = 3,
	};
}
#endif