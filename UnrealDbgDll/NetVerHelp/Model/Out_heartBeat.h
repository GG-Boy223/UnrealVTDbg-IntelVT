#pragma once
#ifndef Out_heartBeat_H_
#define Out_heartBeat_H_
#include <string>
#include "Out_DataBaseArgs.h"
using namespace std;
namespace Model
{
	//心跳出参
	class Out_heartBeat :public Out_DataBaseArgs
	{
	public:
		//心跳Key,每次心跳此值都会变
		string	heartbeatkey;
		//账号或卡密到期时间
		string endtime;

		//账号或卡密剩余点数
		int surpluspointvalue;
	};
}
#endif