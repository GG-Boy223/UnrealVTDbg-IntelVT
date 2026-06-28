#pragma once
#include <string.h>
#include "../Help_Tou/Rc4.h"
namespace Help
{
	char* Rc4::Encrypt(const char* szSource, const char* szPassWord) // 加密，返回加密结果
	{
		if (szSource == NULL || szPassWord == NULL) return NULL;

		unsigned char* ret = new unsigned char[strlen(szSource)];

		int ret_len = 0;

		if (RC4((unsigned char*)szSource,
			strlen(szSource),
			(unsigned char*)szPassWord,
			strlen(szPassWord),
			ret,
			&ret_len) == NULL)
			return NULL;

		char* ret2 = ByteToHex(ret, ret_len);

		delete[] ret;
		return ret2;
	}

	char* Rc4::Decrypt(const char* szSource, const char* szPassWord) // 解密，返回解密结果
	{
		if (szSource == NULL || (strlen(szSource) % 2 != 0) || szPassWord == NULL)
			return NULL;

		unsigned char* src = HexToByte(szSource);

		unsigned char* ret = new unsigned char[strlen(szSource) / 2 + 1];

		int ret_len = 0;

		memset(ret, strlen(szSource) / 2 + 1, 0);

		if (RC4(src, strlen(szSource) / 2, (unsigned char*)szPassWord, strlen(szPassWord), ret, &ret_len) == NULL)
			return NULL;

		ret[ret_len] = '\0';

		return (char*)ret;
	}

	int Rc4::RC4(const unsigned char* data, int data_len, const unsigned char* key, int key_len, unsigned char* out, int* out_len)
	{
		if (data == NULL || key == NULL || out == NULL)
			return NULL;

		unsigned char* mBox = new unsigned char[BOX_LEN];

		if (GetKey(key, key_len, mBox) == NULL)
			return NULL;

		int i = 0;
		int x = 0;
		int y = 0;

		for (int k = 0; k < data_len; k++)
		{
			x = (x + 1) % BOX_LEN;
			y = (mBox[x] + y) % BOX_LEN;
			swap_byte(&mBox[x], &mBox[y]);
			out[k] = data[k] ^ mBox[(mBox[x] + mBox[y]) % BOX_LEN];
		}

		*out_len = data_len;
		// *out_len = data_len;
		delete[] mBox;
		return -1;
	}

	int Rc4::GetKey(const unsigned char* pass, int pass_len, unsigned char* out)
	{
		if (pass == NULL || out == NULL)
			return NULL;

		int i;

		for (i = 0; i < BOX_LEN; i++)
			out[i] = i;

		int j = 0;
		for (i = 0; i < BOX_LEN; i++)
		{
			j = (pass[i % pass_len] + out[i] + j) % BOX_LEN;
			swap_byte(&out[i], &out[j]);
		}

		return -1;
	}

	void Rc4::swap_byte(unsigned char* a, unsigned char* b)
	{
		unsigned char swapByte;

		swapByte = *a;

		*a = *b;

		*b = swapByte;
	}

	// 把字节码转为十六进制码，一个字节两个十六进制，内部为字符串分配空间
	char* Rc4::ByteToHex(const unsigned char* vByte, const int vLen)
	{
		if (!vByte)
			return NULL;

		char* tmp = new char[vLen * 2 + 1]; // 一个字节两个十六进制码，最后要多一个'/0'

		int tmp2;
		for (int i = 0; i < vLen; i++)
		{
			tmp2 = (int)(vByte[i]) / 16;
			tmp[i * 2] = (char)(tmp2 + ((tmp2 > 9) ? 'A' - 10 : '0'));
			tmp2 = (int)(vByte[i]) % 16;
			tmp[i * 2 + 1] = (char)(tmp2 + ((tmp2 > 9) ? 'A' - 10 : '0'));
		}

		tmp[vLen * 2] = '\0';
		return tmp;
	}

	// 把十六进制字符串，转为字节码，每两个十六进制字符作为一个字节
	unsigned char* Rc4::HexToByte(const char* szHex)
	{
		if (!szHex)
			return NULL;

		int iLen = strlen(szHex);

		if (iLen <= 0 || 0 != iLen % 2)
			return NULL;

		unsigned char* pbBuf = new unsigned char[iLen / 2];  // 数据缓冲区

		int tmp1, tmp2;
		for (int i = 0; i < iLen / 2; i++)
		{
			if ((szHex[i * 2] >= 'A') && (szHex[i * 2] <= 'F'))
			{
				tmp1 = (int)szHex[i * 2] - (((int)szHex[i * 2] >= 'A') ? 'A' - 10 : '0');
			}
			else if ((szHex[i * 2] >= 'a') && (szHex[i * 2] <= 'f'))
			{
				tmp1 = (int)szHex[i * 2] - (((int)szHex[i * 2] >= 'a') ? 'a' - 10 : '0');
			}
			else
			{
				tmp1 = (int)szHex[i * 2] - '0';
			}

			if (tmp1 >= 16)
				return NULL;

			if ((szHex[i * 2 + 1] >= 'A') && (szHex[i * 2 + 1] <= 'F'))
			{

				tmp2 = (int)szHex[i * 2 + 1] - (((int)szHex[i * 2 + 1] >= 'A') ? 'A' - 10 : '0');
			}
			else if ((szHex[i * 2 + 1] >= 'a') && (szHex[i * 2 + 1] <= 'f'))
			{
				tmp2 = (int)szHex[i * 2 + 1] - (((int)szHex[i * 2 + 1] >= 'a') ? 'a' - 10 : '0');
			}
			else
			{
				tmp2 = (int)szHex[i * 2 + 1] - '0';
			}

			if (tmp2 >= 16)
				return NULL;

			pbBuf[i] = (tmp1 * 16 + tmp2);
		}

		return pbBuf;
	}
}