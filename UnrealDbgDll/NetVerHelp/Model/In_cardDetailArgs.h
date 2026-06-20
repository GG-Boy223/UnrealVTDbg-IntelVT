#pragma once
#ifndef In_cardDetailArgs_H_
#define In_cardDetailArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;

namespace Model
{
/**
* @class In_cardDetailArgs
* @brief 这是一个用于处理卡密详细信息的类。
* @details 此类继承自 In_DataBaseArgs，用于表示卡片的密钥信息。
* 包含一个卡密（cardnum）成员，这是必填项。
*/
	class In_cardDetailArgs :public In_DataBaseArgs
	{
	public:
		/// <summary>
		///  卡密(必填)
		/// </summary>
		string cardnum;
	};
}
#endif