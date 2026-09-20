#pragma once

#ifndef _INVALID_EPT_H
#define _INVALID_EPT_H

struct __invept_descriptor
{
	unsigned __int64 ept_pointer;
	unsigned __int64 reserved;
};

// 刷新当前逻辑处理器上的所有 EPT 上下文
void invept_all_contexts_func();
// 刷新当前逻辑处理器上指定 EPTP 的 EPT 上下文
void invept_single_context_func(unsigned __int64 ept_pointer);

#endif // !_INVALID_EPT_H
