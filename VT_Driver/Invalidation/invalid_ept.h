#pragma once

#ifndef _INVALID_EPT_H
#define _INVALID_EPT_H

struct __invept_descriptor
{
	unsigned __int64 ept_pointer;
	unsigned __int64 reserved;
};

//刷新全部的逻辑处理器
void invept_all_contexts_func();
//刷新指定的逻辑处理器的eptp
void invept_single_context_func(unsigned __int64 ept_pointer);

#endif // !_INVALID_EPT_H
