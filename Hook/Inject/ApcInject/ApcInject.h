#pragma once

#ifndef _HOOK_APC_INJECT_H
#define _HOOK_APC_INJECT_H

void ApcCallRemoteFunc(HANDLE hProcess);

//通过驱动注入apc线程到目标进程
BOOL _ApcCallRemoteFunc(HANDLE hProcess);

#endif // !_HOOK_APC_INJECT_H
