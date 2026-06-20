#pragma once

#ifndef _INIT_SYMBOLIC_H
#define _INIT_SYMBOLIC_H

// 用于 DbgkSysWin10 -> VT_Driver 初始化握手的固定校验值。
// 这里故意保留为共享常量，而不是到处写魔数，方便后续统一修改。


VOID InitSymbolsTable(IN PUSER_DATA userData, IN PIRP pIrp);

BOOLEAN InitNtoskrnlSymbolsTable();
BOOLEAN InitWin32kbaseSymbolsTable();
BOOLEAN InitWin32kfullSymbolsTable();

////将内核结构的偏移发送给vt host
//bool DispatchOffsetToHost();
//
//void CheckFunctionPointers();

// 将内核结构的偏移发送给 vt host。
// verifyKey 由上层在校验成功后传入，VT 侧会再次校验。
bool DispatchOffsetToHost(unsigned __int64 verifyKey);

void CheckFunctionPointers();

#endif // !_INIT_SYMBOLIC_H
