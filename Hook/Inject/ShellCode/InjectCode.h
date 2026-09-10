#pragma once

#ifndef _INJECT_CODE_H

BOOL InjectCode(HANDLE hProcess);


//摘除钩子
void RemoveHook(DWORD dwPid);

//恢复钩子
void RestoreHook(DWORD dwPid);

//摘除钩子
void RemoveKiUserApcDispatcherHook(DWORD dwPid);

//恢复钩子
void RestoreKiUserApcDispatcherHook(DWORD dwPid);


#endif // !_INJECT_CODE_H
