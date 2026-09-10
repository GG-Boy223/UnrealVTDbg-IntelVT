#pragma once
#ifndef Result_H_
#define Result_H_
#include <string>
using namespace std;
namespace Model
{
	/// <summary>
	/// //接口返回来的数据
	/// </summary>
	class  Result
	{
	public:
		/// <summary>
		/// //接口返回来的消息
		/// </summary>
		string msg;

		/// <summary>
		/// //接口返回业务类型的具体数据
		/// </summary>
		string data;

		/// <summary>
		/// //成功：0  失败：不是0都是失败
		/// </summary>
		int code;

	};
}
#endif