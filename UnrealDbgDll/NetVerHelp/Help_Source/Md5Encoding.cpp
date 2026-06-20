#pragma once

#include "../Help_Tou/Md5.h"

namespace Help
{
	const char MD5::HEX[16] = {
		'0', '1', '2', '3',
		'4', '5', '6', '7',
		'8', '9', 'a', 'b',
		'c', 'd', 'e', 'f'
	};

	void MD5::encode(const uint32* input, byte* output, size_t length)
	{
		for (size_t i = 0, j = 0; j < length; ++i, j += 4)
		{
			output[j] = (byte)(input[i] & 0xff);
			output[j + 1] = (byte)((input[i] >> 8) & 0xff);
			output[j + 2] = (byte)((input[i] >> 16) & 0xff);
			output[j + 3] = (byte)((input[i] >> 24) & 0xff);
		}
	}

	void MD5::decode(const byte* input, uint32* output, size_t length)
	{
		for (size_t i = 0, j = 0; j < length; ++i, j += 4)
		{
			output[i] = ((uint32)input[j]) | (((uint32)input[j + 1]) << 8) |
				(((uint32)input[j + 2]) << 16) | (((uint32)input[j + 3]) << 24);
		}
	}

	string MD5::bytesToHexString(const byte* input, size_t length)
	{
		string str;
		str.reserve(length << 1);
		for (size_t i = 0; i < length; ++i)
		{
			int t = input[i];
			int a = t / 16;
			int b = t % 16;
			str.append(1, HEX[a]);
			str.append(1, HEX[b]);
		}
		return str;
	}

	string MD5::toString()
	{
		return bytesToHexString(digest(), 16);
	}
}
