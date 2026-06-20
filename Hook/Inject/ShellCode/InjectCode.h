#pragma once

#ifndef _INJECT_CODE_H

BOOL InjectCode(HANDLE hProcess);


//Õª³ý¹³×Ó
void RemoveHook(DWORD dwPid);

//»Ö¸´¹³×Ó
void RestoreHook(DWORD dwPid);

//Õª³ý¹³×Ó
void RemoveKiUserApcDispatcherHook(DWORD dwPid);

//»Ö¸´¹³×Ó
void RestoreKiUserApcDispatcherHook(DWORD dwPid);


#endif // !_INJECT_CODE_H
