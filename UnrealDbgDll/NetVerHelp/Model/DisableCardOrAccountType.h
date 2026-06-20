#pragma once
#ifndef DisableCardOrAccountType_H_
#define DisableCardOrAccountType_H_
namespace Model
{
	//禁用还是删除：单码或账号
	enum DisableCardOrAccountType
	{
		// 禁用
		Disable = 0,
		//删除
		Delete = 1,
	};
}
#endif