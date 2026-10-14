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
// 刷新当前逻辑处理器唯一的 EPT 上下文（本核 EPTP）。本项目每核只有一个 EPTP，
// 等价于 all-context 但开销更小，用于统一替代 invept_all_contexts_func()。
void invept_current_context();

#endif // !_INVALID_EPT_H
