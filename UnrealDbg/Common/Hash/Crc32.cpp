#include <iostream>
#include <Windows.h>
#include "../VMProtect/vmp.h"
#include "Crc32.h"

////////////////////////////////////////////////////////////////
// 计算字符串的CRC32值
// 参数：欲计算CRC32值字符串的首地址和大小
// 返回值: 返回CRC32值

DWORD CRC32(BYTE* first_ptr, DWORD Size)
{
	VMProtectionScope vmpScope;

	DWORD crcTable[256], crcTmp1;

	//动态生成CRC-32表
	for (int i = 0; i < 256; i++)
	{
		crcTmp1 = i;
		for (int j = 8; j > 0; j--)
		{
			if (crcTmp1 & 1) crcTmp1 = (crcTmp1 >> 1) ^ 0xEDB88320L;
			else crcTmp1 >>= 1;
		}

		crcTable[i] = crcTmp1;
	}
	//计算CRC32值
	DWORD crcTmp2 = 0xFFFFFFFF;
	while (Size--)
	{
		crcTmp2 = ((crcTmp2 >> 8) & 0x00FFFFFF) ^ crcTable[(crcTmp2 ^ (*first_ptr)) & 0xFF];
		first_ptr++;
	}

	return ~(crcTmp2 ^ 0xFFFFFFFF);
}