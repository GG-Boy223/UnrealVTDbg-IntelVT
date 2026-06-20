#pragma once
#ifndef In_LoginOutArgs_H_
#define In_LoginOutArgs_H_
#include <string>
#include "In_DataBaseArgs.h"
using namespace std;
namespace Model
{
	// ÍË³öµÇÂ¼
	class In_LoginOutArgs :public In_DataBaseArgs
	{
	public:
		//µÇÂ¼ÁîÅÆ(±ØÌî)
		string token;
		// ¿¨ÃÜ»òÕËºÅ(±ØÌî)
		string cardnumorusername;
	};
}
#endif