#pragma once

#ifndef _INIT_SYMBOLIC_H
#define _INIT_SYMBOLIC_H

VOID InitSymbolsTable(IN PUSER_DATA userData, IN PIRP pIrp);

BOOLEAN InitNtoskrnlSymbolsTable();
BOOLEAN InitWin32kbaseSymbolsTable();
BOOLEAN InitWin32kfullSymbolsTable();

// 将内核结构偏移发送给 vt host。
// verifyKey 为上层已经校验通过的握手值；VT_Driver 侧会做二次校验。
bool DispatchOffsetToHost(unsigned __int64 verifyKey);


//Dump对象偏移和函数指针
void DumpOffsetAndFuncPtr();

void CheckFunctionPointers();

#endif // !_INIT_SYMBOLIC_H
