#pragma once

#ifndef _INIT_NTDEVICE_H
#define _INIT_NTDEVICE_H

extern HANDLE g_hGeneralDriverDevice;

//初始化驱动设备
BOOL InitializeDevice(const std::wstring DriveImagePath);

BOOL _Initialize(const TCHAR* sPath);
//结束时的扫尾工作
BOOL UnInitialize();

// 旧项目保留声明：结束阶段额外扫尾入口。
// 注释原因：当前实现已把句柄关闭、UnrealDevice/VT_Driver/BACDevice 停止逻辑集中到 UnInitialize，单独 END 入口没有实现体。
// 放开好处：如果未来拆分“普通卸载”和“最终卸载”，可以复用这个扩展点。
// 放开坏处：现在直接声明并调用会造成未解析符号或空实现误导，影响退出路径可靠性。
// 项目影响：继续注释不会影响驱动卸载；放开前必须补实现并定义调用时机。
// 当前建议：继续注释。
//BOOL UnInitialize_END();

#endif // !_INIT_NTDEVICE_H
