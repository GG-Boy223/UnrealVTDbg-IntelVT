#pragma once
#ifndef In_Out_DataBaseArgs_H_
#define In_Out_DataBaseArgs_H_
#include <string>
using std::string;
using namespace std;
namespace Model
{
	/// <summary>
	/// 请求标识实体类
	/// </summary>
	class Out_DataBaseArgs
	{

	public:
		/*
		请求接口时提交上来的一个标识，会原本原样的把此值返回去。如果返回去的值与当前提交的值不一致，
		说明接口返回的数据有可能被“破解者”截持，那么您的软件可以直接关闭或不做任何处理
		*/
		// /*
		// 请求接口时提交上来的一个标识，会原本原样的把此值返回去。如果返回去的值与当前提交的值不一致，
		// 说明接口返回的数据有可能被“破解者”截持，那么您的软件可以直接关闭或不做任何处理
		// */
		string requestflag;

		/// <summary>
		/// //服务器时间戳
		/// </summary>
		__int64 servertimestamp;
	};
}
#endif