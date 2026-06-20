#pragma once

#include "../Help_Tou/Md5.h"

#include <cstring>

namespace Help
{
	const MD5::byte MD5::PADDING[64] = { 0x80 };

	MD5::MD5()
	{
		reset();
	}

	MD5::MD5(const void* input, size_t length)
	{
		reset();
		update(input, length);
	}

	MD5::MD5(const string& str)
	{
		reset();
		update(str);
	}

	MD5::MD5(ifstream& in)
	{
		reset();
		update(in);
	}

	const MD5::byte* MD5::digest()
	{
		if (!_finished)
		{
			_finished = true;
			final();
		}
		return _digest;
	}

	void MD5::reset()
	{
		_finished = false;
		_count[0] = _count[1] = 0;
		_state[0] = 0x67452301;
		_state[1] = 0xefcdab89;
		_state[2] = 0x98badcfe;
		_state[3] = 0x10325476;
	}

	void MD5::update(const void* input, size_t length)
	{
		update((const byte*)input, length);
	}

	void MD5::update(const string& str)
	{
		update((const byte*)str.c_str(), str.length());
	}

	void MD5::update(ifstream& in)
	{
		if (!in)
		{
			return;
		}

		std::streamsize length;
		char buffer[BUFFER_SIZE];
		while (!in.eof())
		{
			in.read(buffer, BUFFER_SIZE);
			length = in.gcount();
			if (length > 0)
			{
				update(buffer, length);
			}
		}
		in.close();
	}

	void MD5::update(const byte* input, size_t length)
	{
		uint32 i, index, partLen;

		_finished = false;

		index = (uint32)((_count[0] >> 3) & 0x3f);

		if ((_count[0] += ((uint32)length << 3)) < ((uint32)length << 3))
		{
			++_count[1];
		}
		_count[1] += ((uint32)length >> 29);

		partLen = 64 - index;

		if (length >= partLen)
		{
			memcpy(&_buffer[index], input, partLen);
			transform(_buffer);

			for (i = partLen; i + 63 < length; i += 64)
			{
				transform(&input[i]);
			}
			index = 0;
		}
		else
		{
			i = 0;
		}

		memcpy(&_buffer[index], &input[i], length - i);
	}

	void MD5::final()
	{
		byte bits[8];
		uint32 oldState[4];
		uint32 oldCount[2];
		uint32 index, padLen;

		memcpy(oldState, _state, 16);
		memcpy(oldCount, _count, 8);

		encode(_count, bits, 8);

		index = (uint32)((_count[0] >> 3) & 0x3f);
		padLen = (index < 56) ? (56 - index) : (120 - index);
		update(PADDING, padLen);

		update(bits, 8);

		encode(_state, _digest, 16);

		memcpy(_state, oldState, 16);
		memcpy(_count, oldCount, 8);
	}
}
