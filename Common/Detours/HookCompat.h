#pragma once

#ifndef _HOOK_COMPAT_H
#define _HOOK_COMPAT_H

bool HookOn(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread);
bool HookOff(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread);

#endif // !_HOOK_COMPAT_H
