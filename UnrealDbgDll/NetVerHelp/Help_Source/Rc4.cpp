#pragma once
#include <string.h>
#include <limits>
#include "../Help_Tou/Rc4.h"
namespace Help
{
	char* Rc4::Encrypt(const char* szSource, const char* szPassWord) // 加密，返回加密结果
	{
		if (szSource == NULL || szPassWord == NULL) return NULL;

		const size_t source_length = strlen(szSource);
		const size_t password_length = strlen(szPassWord);
		if (source_length > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
			password_length == 0 ||
			password_length > static_cast<size_t>((std::numeric_limits<int>::max)()))
		{
			return NULL;
		}

		unsigned char* ret = new unsigned char[source_length == 0 ? 1 : source_length];

		int ret_len = 0;

		if (RC4((unsigned char*)szSource,
			static_cast<int>(source_length),
			(unsigned char*)szPassWord,
			static_cast<int>(password_length),
			ret,
			&ret_len) == NULL)
		{
			delete[] ret;
			return NULL;
		}

		char* ret2 = ByteToHex(ret, ret_len);

		delete[] ret;
		return ret2;
	}

	char* Rc4::Decrypt(const char* szSource, const char* szPassWord) // 解密，返回解密结果
	{
		if (szSource == NULL || (strlen(szSource) % 2 != 0) || szPassWord == NULL)
			return NULL;

		const size_t source_length = strlen(szSource);
		const size_t password_length = strlen(szPassWord);
		if (source_length / 2 > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
			password_length == 0 ||
			password_length > static_cast<size_t>((std::numeric_limits<int>::max)()))
		{
			return NULL;
		}

		unsigned char* src = HexToByte(szSource);
		if (src == NULL)
		{
			return NULL;
		}

		const size_t decoded_length = source_length / 2;
		unsigned char* ret = new unsigned char[decoded_length + 1];

		int ret_len = 0;

		memset(ret, 0, decoded_length + 1);

		if (RC4(
			src,
			static_cast<int>(decoded_length),
			(unsigned char*)szPassWord,
			static_cast<int>(password_length),
			ret,
			&ret_len) == NULL)
		{
			delete[] src;
			delete[] ret;
			return NULL;
		}

		delete[] src;

		ret[ret_len] = '\0';

		return (char*)ret;
	}

	int Rc4::RC4(const unsigned char* data, int data_len, const unsigned char* key, int key_len, unsigned char* out, int* out_len)
	{
		if (data == NULL || data_len < 0 || key == NULL || key_len <= 0 || out == NULL || out_len == NULL)
			return NULL;

		unsigned char* mBox = new unsigned char[BOX_LEN];

		if (GetKey(key, key_len, mBox) == NULL)
		{
			delete[] mBox;
			return NULL;
		}

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
		return 1;
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

		return 1;
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

		const size_t length = strlen(szHex);
		if (length > static_cast<size_t>((std::numeric_limits<int>::max)()))
			return NULL;

		int iLen = static_cast<int>(length);

		if (iLen <= 0 || 0 != iLen % 2)
			return NULL;

		unsigned char* pbBuf = new unsigned char[iLen / 2];  // 数据缓冲区

		auto decode_nibble = [](const char value) -> int
		{
			if (value >= '0' && value <= '9')
				return value - '0';
			if (value >= 'A' && value <= 'F')
				return value - 'A' + 10;
			if (value >= 'a' && value <= 'f')
				return value - 'a' + 10;
			return -1;
		};

		for (int i = 0; i < iLen / 2; i++)
		{
			const int tmp1 = decode_nibble(szHex[i * 2]);
			const int tmp2 = decode_nibble(szHex[i * 2 + 1]);
			if (tmp1 < 0 || tmp2 < 0)
			{
				delete[] pbBuf;
				return NULL;
			}

			pbBuf[i] = (tmp1 * 16 + tmp2);
		}

		return pbBuf;
	}
}