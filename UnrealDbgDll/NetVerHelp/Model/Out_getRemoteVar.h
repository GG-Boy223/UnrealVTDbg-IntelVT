#pragma once
#ifndef Out_getRemoteVar_H_
#define Out_getRemoteVar_H_
#include <string>
#include <list>
#include "Out_Var.h"
#include "Out_DataBaseArgs.h"
using namespace std;
namespace Model
{
	//远程变量出参
	class Out_getRemoteVar :public Out_DataBaseArgs
	{
	public:
		// 变量值
		string varValue;
	};
}
#endif