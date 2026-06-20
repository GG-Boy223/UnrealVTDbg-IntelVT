#pragma once

#ifndef _CRC32_H
#define _CRC32_H

////////////////////////////////////////////////////////////////
// 计算字符串的CRC32值
// 参数：欲计算CRC32值字符串的首地址和大小
// 返回值: 返回CRC32值

DWORD CRC32(BYTE* first_ptr, DWORD Size);

#endif // !_CRC32_H
