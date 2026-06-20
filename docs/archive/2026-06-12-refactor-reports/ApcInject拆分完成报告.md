# ApcInject 拆分与 x64-only 状态

**更新日期:** 2026-06-12

## 当前结论

`Common/Ring0/Inject/ApcInject` 已完成模块化拆分，并已按当前项目要求固定为 x64-only。旧的 32 位 shellcode、跨架构 APC 注入和 32 位系统目录解析口径已废弃。

WOW64 目标仍会被识别，但用途是拒绝执行并输出诊断，不再作为受支持注入目标。

## 当前结构

```text
Common/Ring0/Inject/ApcInject/
├── ApcInject.cpp
├── ApcInject.h
├── Api/
├── Core/
└── Types/
```

核心职责已经拆到 `Core/`：

- `ApcQueue.cpp`、`ThreadLookup.cpp`：APC 入队、线程筛选、WOW64 目标拒绝。
- `ShellcodeBuilder.cpp`：x64 shellcode 生成。
- `UserModuleLookup.cpp`、`ApiSetResolver.cpp`、`ImagePathResolver.cpp`、`SxsResolver.cpp`：用户模块、API Set、镜像路径、SxS 解析。
- `ExportResolver.cpp`、`RemoteRoutineCall.cpp`：导出解析和远程例程调用。
- `StringUtilities.cpp`、`KernelResolver.cpp`、`ThreadExecution.cpp`、`UtilityFunctions.cpp`：字符串、内核基址、SSDT、工作线程与底层工具。

公共和内部类型已经拆到 `Api/` 与 `Types/`，`ApcInject.cpp` 只保留共享状态和拆分入口说明。

## x64-only 约束

- 入口层拒绝 WOW64 目标。
- APC 队列、线程查找、远程调用、SxS 解析链路均有 WOW64 拒绝 gate。
- Shellcode 生成只保留 x64 调用序列。
- 用户模块路径解析固定 System32，不再走 32 位系统目录。
- PE 导出解析只接受 PE64。

## 验证状态

- `build_dbgk_win10_release_x64.bat`：通过，0 warning / 0 error。
- `build_dbgk_win11_release_x64.bat`：通过，0 warning / 0 error。

Delphi 和 `UnrealDbgDll` 相关环境限制不影响此内核 APC 注入链的 x64 构建验证结论。
