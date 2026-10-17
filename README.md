# UnrealVTDbg / F3 VT Debugger

[![QQ 群](https://img.shields.io/badge/QQ-VT_Free-12B7F5?style=for-the-badge&logo=tencentqq&logoColor=white)](https://qm.qq.com/q/JR1i60Ii8C)
[![Telegram](https://img.shields.io/badge/Telegram-F3_VT-26A5E4?style=for-the-badge&logo=telegram&logoColor=white)](https://t.me/F3_vt)

`UnrealVTDbg` 是一套面向 Windows 10/11 x64 的 Intel VT-x 内核调试系统。项目通过 VMX Root Mode、VMCS、EPT、VPID、VMCALL、Windows 调试子系统 Hook 和用户态 WinUI 3 前端，形成从界面、后端桥接、DbgkSys 内核驱动到 VT_Driver 虚拟化引擎的完整调试链路。

> 本项目直接操作内核、调试对象、EPT 和 VMX 状态。只应在拥有明确授权的测试环境、虚拟机或专用实验设备中使用。错误加载驱动、Hook 选择不当或与安全软件冲突，可能导致蓝屏、数据损坏、目标进程崩溃或系统无法启动。

## 项目概览

当前仓库已完成以下结构调整：

- WinUI 3 前端 `UnrealDbgWinUI` 替代旧 Delphi/VCL 主界面。
- 旧 Delphi 调试器、卡密工具、加密工具、SymbolTool 和旧 Loader 已迁入 `历史UI/`。
- 原独立 `Loader.exe` 不再参与构建，PDB 定位、校验、下载和符号分析已合并到 WinUI。
- `UnrealDbg.sln` 当前只包含 6 个 C++ 工程：`UnrealDbgWinUI`、`UnrealDbgDll`、`Hook`、`DbgkSysWin10`、`DbgkSysWin11`、`VT_Driver`。
- DbgkSys 的 EPT Hook 已支持 36 位掩码选择，并可通过 WinUI 在执行 VT 初始化前逐项开关。
- Hook 掩码通过 `ConfigureHooks_F3`、`IOCTL_CONFIGURE_EPT_HOOKS`、`RING3_HOOK_CONFIGURATION` 和 DbgkSys `SetupEptHook()` 形成完整下发链。

## 阅读导航

- [核心能力](#核心能力)
- [系统架构](#系统架构)
- [解决方案组成](#解决方案组成)
- [仓库结构](#仓库结构)
- [启动与初始化](#启动与初始化)
- [核心模块](#核心模块)
- [EPT Hook 选择机制](#ept-hook-选择机制)
- [全部 Hook 详细清单](#全部-hook-详细清单)
- [Hook 安装顺序与平台差异](#hook-安装顺序与平台差异)
- [构建](#构建)
- [部署与运行](#部署与运行)
- [配置与日志](#配置与日志)
- [验证结果](#验证结果)
- [兼容性与限制](#兼容性与限制)
- [故障排查](#故障排查)
- [开发约束](#开发约束)

## 核心能力

### Intel VT-x

- 检查 CPUID、`IA32_FEATURE_CONTROL` 和 VMX capability MSR。
- 执行 VMXON、VMCS 初始化、VMLAUNCH、VMRESUME 和 VMXOFF。
- 按硬件能力调整 Pin-Based、Primary、Secondary、VM-entry 和 VM-exit 控制位。
- 维护 Guest/Host 的 CR0、CR3、CR4、EFER、GDTR、IDTR、TR、段状态、调试状态和 MSR 区域。
- 处理 VM-entry failure、VM-exit failure、VM-instruction error 和清理路径。

### EPT

- 构建 PML4、PDPT、PD、PT 四级 EPT 页表。
- 当前实现以 identity mapping 为主，并保留 MTRR 缓存类型映射。
- 支持 2 MB 大页到 4 KB 小页拆分。
- 支持页级读、写、执行权限控制。
- 支持 EPT Hook、EPT Watch、执行监控、读写监控和 MTF 权限恢复。
- 使用 INVEPT single-context 或 all-context 刷新地址转换缓存。

### 调试子系统

- 重建 Windows 调试对象和调试事件路径。
- 支持进程附加、事件等待、事件继续和进程分离。
- 处理进程、线程、模块、异常等调试事件。
- 通过 EPT Hook 接管 `ntoskrnl`、`win32k` 和对象管理相关函数。
- 对进程调试端口、调试对象句柄、线程上下文和系统枚举进行可见性处理。

### 调试语义

- INT3、CC Hook、INT1 Hook、VMCALL/RIP Hook 和函数 Hook。
- EPT Watch 与 MTF 配合处理执行、读、写断点。
- 隐形软件断点使用伪页隔离原始代码与修改代码。
- 处理 DR0-DR7 保存、恢复和异常注入。
- 支持 Step Into、Step Over、Step Out 所需的断点、单步和异常组合路径。

### WinUI 3

- 总览、调试器、目标与防护、驱动 Hook、日志、系统详情、系统符号、诊断页面。
- 明亮与暗黑主题。
- 外部调试器登记、启动和路径管理。
- PDB 缓存、GUID/Age 校验、符号下载和函数/类型/RVA 解析。
- Hook 掩码在初始化前可编辑，初始化开始后自动锁定。

## 系统架构

```text
+-----------------------------------------------------------------------+
|                              Ring 3                                   |
|                                                                       |
|  UnrealDbgWinUI.exe                                                   |
|    - WinUI 3 / C++/WinRT                                              |
|    - 配置、日志、目标捕获                                               |
|    - PDB 准备与符号分析                                                |
|    - Hook 选择与初始化控制                                             |
|                              |                                        |
|                              | LoadLibrary / GetProcAddress            |
|                              v                                        |
|  UnrealDbgDll.dll                                                     |
|    - Initialize_F3                                                    |
|    - ConfigureHooks_F3                                                |
|    - StartProcess / GetFileVersion                                    |
|    - TL_BlockGameResumeThread                                         |
|    - IOCTL 封装、用户数据加密、卡密/网络验证                           |
|                              |                                        |
|                              | DeviceIoControl(\??\UnrealDbg)          |
+------------------------------|----------------------------------------+
                               v
+-----------------------------------------------------------------------+
|                              Ring 0                                   |
|                                                                       |
|  DbgkSysWin10.sys / DbgkSysWin11.sys                                  |
|    - 调试对象与调试事件                                               |
|    - 进程/线程/模块/异常事件                                          |
|    - 36 位 EPT Hook 选择                                              |
|    - 反调试查询与对象保护                                             |
|    - HypervisorGateway DPC 广播                                       |
|                              |                                        |
|                              | VMCALL                                  |
|                              v                                        |
|  VT_Driver.sys                                                        |
|    - VMXON / VMCS / VMLAUNCH / VMRESUME / VMXOFF                      |
|    - VM-exit dispatcher                                               |
|    - EPT / INVEPT / VPID / INVVPID                                    |
|    - CR3 Tracker                                                      |
|    - EPT Hook / EPT Watch / Fake Page / MTF                           |
|    - Pool Manager / Host Page Tables / MTRR                           |
|                              |                                        |
|                              | VM-entry / VM-exit                      |
+------------------------------|----------------------------------------+
                               v
+-----------------------------------------------------------------------+
|                         Windows Guest                                 |
|  Windows 10/11 x64、目标进程、外部调试器、被保护组件                  |
+-----------------------------------------------------------------------+
```

## 解决方案组成

| 工程 | 类型 | 主要职责 |
|---|---|---|
| `UnrealDbgWinUI` | WinUI 3 应用 | 当前主界面、符号准备、配置、日志和初始化入口 |
| `UnrealDbgDll` | x64 DLL | 前端与驱动桥接、驱动加载、IOCTL、符号握手、外部调试器启动 |
| `Hook` | x64 DLL | 用户态 Hook、断点透明化、调试器和目标侧兼容逻辑 |
| `DbgkSysWin10` | WDK 驱动 | Windows 10 调试子系统、EPT Hook、IOCTL 与 VMCALL 广播 |
| `DbgkSysWin11` | WDK 驱动 | Windows 11 调试子系统，结构对应 Win10 工程 |
| `VT_Driver` | WDK 驱动 | Intel VT-x VMM、VMCS、EPT、CR3、VM-exit 和内存池 |

`历史UI/` 不属于当前解决方案，只用于旧界面、旧工具、旧配置和旧行为参考。

## 仓库结构

```text
Intel-VT/
|-- README.md
|-- UnrealDbg.sln
|-- Common/
|   |-- Shared/
|   |   |-- HookSelection.h
|   |   |-- IOCTLs.h
|   |   `-- SharedStruct.h
|   |-- Ring0/
|   |-- Detours/
|   |-- Encrypt/
|   |-- FileSystem/
|   |-- Hash/
|   |-- IPC/
|   |-- Logger/
|   |-- TinyXML/
|   `-- VMProtect/
|-- UnrealDbgWinUI/
|   |-- Main.cpp
|   |-- Runtime.cpp
|   |-- Bridge.h
|   |-- SymbolPreparer.cpp
|   |-- SystemInspector.cpp
|   |-- UiComponents.cpp
|   `-- UiTheme.h
|-- UnrealDbgDll/
|   |-- Channels/
|   |-- Init/
|   |-- Interface/
|   |-- NetVerHelp/
|   |-- StartProcess.cpp
|   |-- dllmain.cpp
|   `-- dllexport.def
|-- Hook/
|   |-- Channels/
|   |-- Core/
|   |-- DebugBreak/
|   |-- DebugEvent/
|   |-- DllMain/
|   |-- Hook/
|   |-- HookCallSet/
|   |-- Inject/
|   |-- NativeApi/
|   `-- vmx/
|-- DbgkSysWin10/
|-- DbgkSysWin11/
|-- VT_Driver/
|   |-- Vmm/
|   |-- Vmcs/
|   |-- VmExit/
|   |-- VmCall/
|   |-- Ept/
|   |-- Cr3/
|   |-- Invalidation/
|   |-- PoolManager/
|   |-- HypervisorGateway/
|   |-- HypervisorRoutines/
|   |-- Interrupts/
|   |-- Processor/
|   |-- Tables/
|   `-- ASM/
|-- 历史UI/
|-- _artifacts/       本地构建依赖，默认忽略
`-- x64/              本地构建产物，默认忽略
```

## 启动与初始化

### 前端启动

```text
UnrealDbgWinUI.exe
  -> 读取 Config.ini
  -> 读取 DebuggerList.ini
  -> 加载 UnrealDbgDll.dll
  -> 解析 5 个后端导出
  -> 读取 [HookSelection] Mask
  -> 后台准备系统 PDB
  -> 展示主界面
```

### 进入 VT 调试模式

```text
用户点击进入 VT
  -> WinUI 锁定 Hook 开关
  -> ConfigureHooks_F3(mask)
  -> UnrealDbgDll 保存 mask
  -> Initialize_F3(key)
       -> 加载 AIHelper.dll
       -> 安装并启动 VT_Driver.sys
       -> 根据 Build 选择 DbgkSysWin10/11
       -> 打开 \??\UnrealDbg
       -> IOCTL_CONFIGURE_EPT_HOOKS
       -> IOCTL_LOAD_SYMBOLS_TABLE
       -> 登记宿主调试器
```

### DbgkSys 到 Hypervisor

```text
DbgkSys IOCTL / 内核 Hook
  -> HypervisorGateway
  -> KeGenericCallDpc
  -> 当前逻辑处理器执行 VMCALL
  -> VT_Driver VM-exit
  -> VmcallDispatch
  -> EPT / CR3 / 断点 / 可见性操作
  -> 返回 Guest
```

### EPT 命中

```text
Guest 访问 Hook 页
  -> EPT violation
  -> VM-exit
  -> EPT violation handler
  -> 查找 Hook/Watch 状态
  -> 切换原页或伪页
  -> 记录 Guest 上下文
  -> 设置 MTF
  -> VMRESUME
  -> MTF VM-exit
  -> 恢复 EPT 权限
  -> 返回调试事件
```

## 核心模块

### UnrealDbgWinUI

`UnrealDbgWinUI` 是当前唯一主界面。

| 页面 | 关键内容 |
|---|---|
| 总览 | VT 状态、后端状态、目标进程、系统环境 |
| 调试器 | 外部调试器添加、删除、定位和启动 |
| 目标与防护 | 前台目标捕获、TL 开关、线程恢复阻塞 |
| 驱动 Hook | 36 个 Hook 的逐项开关、全开、全关 |
| 日志中心 | 内部日志、VTLoad、Hook64、DbgkSys 日志 |
| 系统详情 | Windows、CPU、内存、固件、安全启动、主板、GPU、磁盘 |
| 系统符号 | PDB 解析、函数/变量/RVA/结构偏移、全局搜索 |
| 诊断与文件 | 运行文件检查、后端路径、加载错误和日志入口 |

关键源码：

- `UnrealDbgWinUI/Main.cpp`：窗口、页面、Hook 描述表和交互。
- `UnrealDbgWinUI/Runtime.cpp`：后端加载、Hook 掩码、配置、符号准备和状态。
- `UnrealDbgWinUI/Bridge.h`：运行状态和函数指针。
- `UnrealDbgWinUI/SymbolPreparer.cpp`：PE 签名、PDB 校验、下载和解析。
- `UnrealDbgWinUI/SystemInspector.cpp`：系统详情采集。

### UnrealDbgDll

当前稳定导出：

| 序号 | 导出 | 作用 |
|---:|---|---|
| 1 | `Initialize_F3` | 执行后端初始化与驱动加载 |
| 2 | `StartProcess` | 启动外部调试器并处理 Hook64 相关流程 |
| 3 | `GetFileVersion` | 读取文件版本 |
| 4 | `TL_BlockGameResumeThread` | 下发阻塞线程恢复请求 |
| 5 | `ConfigureHooks_F3` | 保存 DbgkSys Hook 选择掩码 |

关键行为：

- `ConfigureHooks_F3` 拒绝 `HOOK_SELECTION_ALL` 之外的未知位。
- Hook 掩码保存在用户态原子变量中。
- `LoadSymbolsTable` 在加载符号和启动后端前发送 Hook 配置。
- `Initialize_F3` 失败时调用后端清理路径。
- `AIHelper.dll` 提供 `LoadNT`、`UnloadNT` 和 `outDebug`，不是当前解决方案的构建产物。

### DbgkSysWin10 / DbgkSysWin11

两个驱动结构基本对应，但分别支持不同的 Windows 内核版本。

设备名称：

```text
\Device\DbgkSysDevice
\??\UnrealDbg
```

系统选择：

```text
Windows Build >= 22000  -> DbgkSysWin11.sys
Windows Build <  22000  -> DbgkSysWin10.sys
```

主要目录：

```text
DbgkSysWin10/
|-- Core/
|-- DbgkApi/
|-- Hooks/EptHook/
|-- Hvm/
|-- Init/
|-- Process/
|-- Protect/
|-- Globals/
|-- Log/
`-- ntos/
```

### VT_Driver

| 目录 | 职责 |
|---|---|
| `Vmm/` | VMM 上下文、VCPU、处理器初始化、Host 页表 |
| `Vmcs/` | VMCS 字段、Guest/Host 状态、控制位和访问封装 |
| `VmExit/` | VM-exit 分发、EPT、MSR、IO、异常、调试寄存器 |
| `VmCall/` | VMCALL 参数解析、前置检查、命令执行和策略 |
| `Ept/` | EPT 构建、拆分、同步、Hook、Watch、伪页 |
| `Cr3/` | CR3 目标、地址转换、工作线程和退出策略 |
| `Invalidation/` | INVEPT、INVVPID、INVPCID |
| `PoolManager/` | VM-exit 路径的预分配内存池 |
| `HypervisorGateway/` | Guest 驱动到 Hypervisor 的网关 |
| `HypervisorRoutines/` | VMX、上下文、中断、位图、VMCS dump |
| `Interrupts/` | NMI 与其他中断广播 |
| `Processor/` | CR、DR、MSR、CPUID、段和 MTRR |

### Hook64.dll

`Hook` 工程构建为 `Hook64.dll`，用于外部调试器和目标侧兼容：

- 用户态 API Hook。
- 隐藏 INT3 断点读取、写入和删除行为。
- 同步断点状态。
- 注入与调试事件辅助。

外部调试器可以被加入列表并启动，但“启动成功”不等于“调试语义全部兼容”。实际行为需要在目标架构、调试器版本和 Windows Build 上逐项验证。

## EPT Hook 选择机制

### 新增能力概述

这次调整不是单独散落几个开关，而是为 DbgkSys 当前使用的 36 个 Hook 入口建立了统一选择契约：

```text
WinUI 开关
  -> 36 位 mask
  -> 配置文件持久化
  -> UnrealDbgDll 导出
  -> 加密 USER_DATA
  -> IOCTL_CONFIGURE_EPT_HOOKS
  -> DbgkSys 校验 version/size/mask
  -> SetupEptHook 按位安装
```

### 二进制接口

```cpp
#define HOOK_CONFIGURATION_VERSION 1U

typedef struct _RING3_HOOK_CONFIGURATION
{
    unsigned long version;
    unsigned long size;
    unsigned __int64 mask;
} RING3_HOOK_CONFIGURATION;
```

在 Windows x64 下：

| 偏移 | 大小 | 字段 |
|---:|---:|---|
| 0 | 4 | `version` |
| 4 | 4 | `size` |
| 8 | 8 | `mask` |
| 合计 | 16 | `RING3_HOOK_CONFIGURATION` |

### IOCTL

```cpp
#define IOCTL_CONFIGURE_EPT_HOOKS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS)
```

传输特点：

- 使用现有 `SendUserDataToDriver`。
- 输入先封装为 `USER_DATA`。
- `RING3_HOOK_CONFIGURATION` 被加密后放入用户数据区。
- 驱动通过 `ResolveInputUserData` 和 `AllocateAndDecryptUserRecords` 解密。
- IOCTL 与共享结构均定义在 `Common/Shared/`。

### 校验规则

| 校验点 | 规则 |
|---|---|
| WinUI | 拒绝 `HOOK_SELECTION_ALL` 之外的位 |
| UnrealDbgDll | `ConfigureHooks_F3` 再次拒绝未知位 |
| DbgkSys | `version == HOOK_CONFIGURATION_VERSION` |
| DbgkSys | `size == sizeof(RING3_HOOK_CONFIGURATION)` |
| DbgkSys | `(mask & ~HOOK_SELECTION_ALL) == 0` |

### 生命周期

```text
启动
  -> 读取配置
  -> 用户编辑 Hook
  -> 进入 VT 前锁定
  -> DLL 保存 mask
  -> DbgkSys 保存 mask
  -> SetupEptHook 按位安装
  -> 当前会话保持
  -> 卸载时 ept_unhook 整体回收
```

当前设计只允许在初始化前修改 Hook 选择。初始化开始后，WinUI 开关会锁定。运行中再次调用配置 IOCTL 可以改变驱动保存的 mask，但不会重新安装或卸载已经安装的当前会话 Hook。

### 默认值

```text
HOOK_SELECTION_ALL = 0x0000000FFFFFFFFF
```

默认表示启用全部已定义位。

- Win10 当前安装路径覆盖全部 36 位。
- Win11 当前 WinUI 只显示 34 个可支持项，`PspCallThreadNotifyRoutines` 和 `NtUserBuildHwndList` 不安装。
- Win11 配置中即使保留这两个位，也不会产生对应 Hook。
- 掩码为 `0` 时不安装 Hook。

## 全部 Hook 详细清单

### 分组一：调试会话、进程、线程与事件

| Bit | Mask | Hook 函数 | 作用 | Win11 |
|---:|---|---|---|---|
| 0 | `1ULL << 0` | `Hook_NtCreateDebugObject` | 创建虚拟调试对象，是调试会话入口；源码要求该 Hook 在组内最先安装 | 支持 |
| 1 | `1ULL << 1` | `Hook_PspInsertProcess` | 接管进程插入路径，把进程生命周期与调试对象关联 | 支持 |
| 2 | `1ULL << 2` | `Hook_NtSetInformationDebugObject` | 处理调试对象信息设置、附加参数和目标关联 | 支持 |
| 3 | `1ULL << 3` | `Hook_NtRemoveProcessDebug` | 接管调试对象移除和进程分离 | 支持 |
| 4 | `1ULL << 4` | `Hook_NtDebugActiveProcess` | 接管进程附加请求，建立虚拟调试关系 | 支持 |
| 5 | `1ULL << 5` | `Hook_NtWaitForDebugEvent` | 接管调试事件等待和事件投递 | 支持 |
| 6 | `1ULL << 6` | `Hook_NtDebugContinue` | 接管继续调试事件，恢复目标线程 | 支持 |
| 7 | `1ULL << 7` | `Hook_DbgkMapViewOfSection` | 生成 DLL/模块加载调试事件 | 支持 |
| 8 | `1ULL << 8` | `Hook_DbgkUnMapViewOfSection` | 生成 DLL/模块卸载调试事件 | 支持 |
| 9 | `1ULL << 9` | `Hook_DbgkCreateThread` | 生成线程创建调试事件 | 支持 |
| 10 | `1ULL << 10` | `Hook_DbgkExitThread` | 生成线程退出事件并完成相关清理 | 支持 |
| 11 | `1ULL << 11` | `Hook_DbgkExitProcess` | 生成进程退出调试事件 | 支持 |
| 12 | `1ULL << 12` | `Hook_DbgkForwardException` | 接管调试异常转发和事件注入 | 支持 |
| 13 | `1ULL << 13` | `Hook_DbgkpQueueMessage` | 接管调试消息队列和事件投递路径 | 支持 |
| 14 | `1ULL << 14` | `Hook_PspCallThreadNotifyRoutines` | 接管线程通知回调，补充 Windows 10 底层线程事件路径 | 不支持 |
| 15 | `1ULL << 15` | `Hook_PspExitThread` | 处理线程退出、调试端口比较和挂起状态清理 | 支持 |

### 分组二：调试对象句柄与对象引用保护

| Bit | Mask | Hook 函数 | 作用 | Win11 |
|---:|---|---|---|---|
| 16 | `1ULL << 16` | `Hook_ObpReferenceObjectByHandleWithTag` | 保护带 Tag 的对象引用，减少虚拟调试对象关联暴露 | 支持 |
| 17 | `1ULL << 17` | `Hook_NtClose` | 接管调试对象句柄关闭，避免会话提前失效 | 支持 |
| 18 | `1ULL << 18` | `Hook_NtDuplicateObject` | 保护调试对象句柄复制和查询 | 支持 |

### 分组三：调试端口、查询与上下文可见性

| Bit | Mask | Hook 函数 | 作用 | Win11 |
|---:|---|---|---|---|
| 19 | `1ULL << 19` | `Hook_DbgkOpenProcessDebugPort` | 接管调试端口打开路径 | 支持 |
| 20 | `1ULL << 20` | `Hook_NtQueryInformationProcess` | 处理 `DebugPort`、`DebugObjectHandle` 等进程调试信息 | 支持 |
| 21 | `1ULL << 21` | `Hook_NtSetInformationProcess` | 接管进程调试信息设置 | 支持 |
| 22 | `1ULL << 22` | `Hook_NtQueryObject` | 处理对象类型和调试对象查询 | 支持 |
| 23 | `1ULL << 23` | `Hook_NtQuerySystemInformation` | 处理系统信息枚举中的调试状态暴露 | 支持 |
| 24 | `1ULL << 24` | `Hook_NtSystemDebugControl` | 接管系统调试控制请求 | 支持 |
| 25 | `1ULL << 25` | `Hook_NtSetInformationThread` | 处理 `HideFromDebugger` 等线程调试标志 | 支持 |
| 26 | `1ULL << 26` | `Hook_NtQueryInformationThread` | 接管线程调试相关信息查询 | 支持 |
| 27 | `1ULL << 27` | `Hook_NtGetContextThread` | 处理线程上下文读取 | 支持 |
| 28 | `1ULL << 28` | `Hook_NtSetContextThread` | 接管线程上下文写入 | 支持 |
| 29 | `1ULL << 29` | `Hook_NtCreateThreadEx` | 接管远程线程创建并关联调试事件 | 支持 |

### 分组四：win32k 窗口查询与枚举

| Bit | Mask | Hook 函数 | 作用 | Win11 |
|---:|---|---|---|---|
| 30 | `1ULL << 30` | `Hook_ValidateHwnd` | 窗口句柄验证入口；源码要求该 Hook 在 win32k 组内最先安装 | 支持 |
| 31 | `1ULL << 31` | `Hook_NtUserFindWindowEx` | 接管按窗口类和标题查找 | 支持 |
| 32 | `1ULL << 32` | `Hook_NtUserWindowFromPoint` | 接管坐标窗口查询 | 支持 |
| 33 | `1ULL << 33` | `Hook_NtUserSetWindowsHookEx` | 接管全局窗口 Hook 注册 | 支持 |
| 34 | `1ULL << 34` | `Hook_NtUserBuildHwndList` | 接管窗口列表枚举，可过滤受保护窗口 | 不支持 |
| 35 | `1ULL << 35` | `Hook_NtUserGetClassName` | 接管窗口类名查询 | 支持 |

## Hook 安装顺序与平台差异

### 实际安装顺序

`SetupEptHook()` 当前按以下顺序检查掩码并调用 Hook：

```text
0  NtCreateDebugObject
1  PspInsertProcess
2  NtSetInformationDebugObject
3  NtRemoveProcessDebug
4  NtDebugActiveProcess
5  NtWaitForDebugEvent
6  NtDebugContinue
7  DbgkMapViewOfSection
8  DbgkUnMapViewOfSection
9  DbgkCreateThread
10 DbgkExitThread
11 DbgkExitProcess
12 DbgkForwardException
13 DbgkpQueueMessage
14 PspCallThreadNotifyRoutines      Win10 only
15 PspExitThread
16 ObpReferenceObjectByHandleWithTag
17 NtClose
18 NtDuplicateObject
19 DbgkOpenProcessDebugPort
20 NtQueryInformationProcess
21 NtSetInformationProcess
22 NtQueryObject
23 NtQuerySystemInformation
24 NtSystemDebugControl
25 NtSetInformationThread
26 NtQueryInformationThread
27 NtGetContextThread
28 NtSetContextThread
29 NtCreateThreadEx
30 ValidateHwnd
31 NtUserFindWindowEx
32 NtUserWindowFromPoint
33 NtUserSetWindowsHookEx
34 NtUserBuildHwndList            Win10 only
35 NtUserGetClassName
```

### 明确顺序约束

- `NtCreateDebugObject`：源码注释要求它是调试对象 Hook 中第一个安装的入口。
- `ValidateHwnd`：源码注释要求它是 win32k 窗口 Hook 中第一个安装的入口。
- 其它 Hook 当前按源码固定顺序调用，但不额外声明相互依赖。

### Windows 10 与 Windows 11 差异

| Bit | 函数 | Win10 | Win11 | 处理方式 |
|---:|---|---|---|---|
| 14 | `PspCallThreadNotifyRoutines` | 安装 | 不安装 | Win11 WinUI 隐藏 |
| 34 | `NtUserBuildHwndList` | 安装 | 不安装 | Win11 WinUI 隐藏 |

Win11 页面的“已启用/支持数”以 34 个可支持 Hook 为基数。配置文件中残留 bit 14/34 不会触发安装。

### 推荐选择

以下组合用于明确调试目标，不是兼容性保证：

| 场景 | 建议 |
|---|---|
| 最小调试事件链 | 核心组 0-13 和 15；Win10 可按需要加 14 |
| 标准调试对象保护 | 核心组 + 16-18 |
| 降低调试可见性 | 核心组 + 16-29 |
| 窗口指纹处理 | 30-33、35；Win10 可加 34 |
| 完整功能 | Win10 全 36 位；Win11 当前支持的 34 位 |
| 只测试初始化 | 可先关闭全部 Hook，确认驱动和 VMX 链正常 |

如果关闭 `NtCreateDebugObject`、`NtDebugActiveProcess`、`NtWaitForDebugEvent` 或 `NtDebugContinue`，调试会话建立和事件传递可能不完整。这是功能选择结果，不是 Hook 安装失败。

## VT_Driver 与 EPT 细节

### VMX 初始化

- 通过 capability MSR 计算控制位的 allowed-0/allowed-1。
- 检查 EPT、VPID、unrestricted guest 等真实能力。
- 为每个逻辑处理器维护 VCPU 和 VMCS。
- 检查 VMCS revision、对齐、VMCLEAR、VMPTRLD 和 ownership。
- 处理 VM-entry/VM-exit 失败和 VMXOFF 清理。

### EPT 页表

```text
EPTP
  -> EPT PML4
       -> EPT PDPT
            -> EPT PD
                 -> 2 MB large page
                 or 4 KB EPT PT
                      -> 512 x 4 KB entries
```

拆分 Hook 页时，未修改的相邻 4 KB 页应继续保留原始物理地址、权限、缓存类型和相关属性。

### EPT Hook

- 保存原始页内容。
- 创建伪页并写入修改后的代码。
- 原始页和伪页使用不同读写执行权限。
- 执行目标地址时进入执行 Hook 路径。
- 读写目标地址时切换到原始页视图。
- MTF 用于在单条指令执行后恢复权限。
- 卸载时恢复 EPT 映射并刷新相关 TLB。

### EPT Watch

- 写监控：关闭 EPT Write。
- 读写监控：关闭 EPT Read 和 Write。
- 执行监控：关闭 EPT Execute。
- 命中后记录虚拟地址、物理地址、断点类型、组 ID 和 Guest 上下文。

### CR3

- CR3 来源包括 Guest 当前 CR3、VMCS Guest CR3 和 CR3 Tracker 记录。
- CR3 改变会影响虚拟地址到 Guest 物理地址的翻译。
- CR3 变化后必须检查翻译缓存、断点地址、EPT Hook 和 Address Monitor 是否仍属于目标地址空间。
- CR3、VPID、EPTP、TLB 是不同概念，不能把 CR3 改变自动等同于 VPID 或 EPTP 改变。

### 失效

- EPT 映射变化后使用 INVEPT。
- VPID 地址转换变化后按实际需要选择 INVVPID。
- 跨核操作通过 DPC/广播保证每个处理器执行所需失效。
- 不能机械地对所有情况使用同一种失效范围，也不能完全不刷新。

## 构建

### 环境要求

| 项目 | 要求 |
|---|---|
| 主机系统 | Windows 10/11 x64 |
| IDE | Visual Studio 2022 |
| C++ 工具集 | MSVC v143 |
| Windows SDK | 10.0.26100.0 |
| WDK | 10.0.26100.0 |
| 汇编器 | MASM `ml64.exe` |
| UI 构建依赖 | Windows App SDK 2.5.1 |
| C++/WinRT | 3.0.260818.1 |
| UI 运行库 | Windows App Runtime 2.5.1 x64 |

WinUI 依赖目录：

```text
_artifacts\winui_251_packages\sdk
_artifacts\winui_251_packages\cppwinrt
```

依赖缺失时会在 `ValidateWinUIPackages` 阶段报错。

### 全解决方案

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

& $msbuild .\UnrealDbg.sln `
  /m `
  /t:Build `
  /p:Configuration=Release `
  /p:Platform=x64
```

### 单独构建 WinUI

```powershell
& $msbuild .\UnrealDbgWinUI\UnrealDbgWinUI.vcxproj `
  /m `
  /p:Configuration=Release `
  /p:Platform=x64
```

WinUI 工程不包含其它工程的 `ProjectReference`。单独构建 UI 时只生成前端，后端驱动、DLL 和 Hook 需要单独构建或通过全解决方案构建。

### 构建产物

| 产物 | 默认路径 |
|---|---|
| `UnrealDbgWinUI.exe` | `x64\WinUI\Release\UnrealDbgWinUI.exe` |
| `UnrealDbgDll.dll` | `x64\Release\UnrealDbgDll.dll` |
| `Hook64.dll` | `x64\Release\Hook64.dll` |
| `VT_Driver.sys` | `x64\Release\VT_Driver.sys` |
| `DbgkSysWin10.sys` | `x64\Release\DbgkSysWin10.sys` |
| `DbgkSysWin11.sys` | `x64\Release\DbgkSysWin11.sys` |

WinUI 的 `StageRuntime` 会把已经存在的运行文件复制到：

```text
x64\WinUI\Release\
x64\WinUI\Release\bin\
```

## 部署与运行

建议发布目录：

```text
Release/
|-- UnrealDbgWinUI.exe
|-- UnrealDbgDll.dll
|-- Hook64.dll
|-- VT_Driver.sys
|-- DbgkSysWin10.sys
|-- DbgkSysWin11.sys
|-- AIHelper.dll
|-- VMProtectSDK64.dll
|-- Microsoft.WindowsAppRuntime.Bootstrap.dll
|-- Config.ini
|-- DebuggerList.ini
|-- Config/
|-- Log/
|-- Symbols/
|-- SymbolTool/
|   `-- Config.ini
`-- bin/
    |-- UnrealDbgDll.dll
    |-- Hook64.dll
    |-- VT_Driver.sys
    |-- DbgkSysWin10.sys
    `-- DbgkSysWin11.sys
```

运行要求：

- 全部运行文件使用 x64。
- `AIHelper.dll` 与当前 `UnrealDbgDll.dll` ABI 匹配。
- 根目录与 `bin/` 下的文件版本一致。
- 驱动已签名，或测试机已正确启用测试签名和内核调试配置。
- 以管理员身份启动 UI，确保 `SeDebugPrivilege` 可用。
- 测试机具备可回滚快照或恢复方式。

快速启动：

1. 构建 `Release|x64`。
2. 准备 `AIHelper.dll`、Windows App Runtime 和运行文件。
3. 以管理员身份启动 `UnrealDbgWinUI.exe`。
4. 在“驱动 Hook”页面选择需要安装的入口。
5. 等待系统 PDB 准备完成。
6. 进入 VT 调试模式。
7. 确认 VT_Driver 和匹配的 DbgkSys 驱动启动。
8. 添加或启动外部调试器。
9. 捕获目标进程并开始调试。

Hook 选择在进入 VT 后锁定。需要变更 Hook 时，应结束当前会话、卸载驱动并重新初始化。

## 配置与日志

### Config.ini

WinUI 优先读取发布目录根部的 `Config.ini`，再读取 `Config\Config.ini`。

```ini
[TL]
enabled_tl_confrontation=0
handler_gettickcount_check=1
BlockResumeThread=0

[HookSelection]
Mask=0x0000000FFFFFFFFF

[UI]
Theme=light
```

| 节 | 键 | 说明 |
|---|---|---|
| `TL` | `enabled_tl_confrontation` | 启用 TL 防护 |
| `TL` | `handler_gettickcount_check` | 启用 tick count 相关检查 |
| `TL` | `BlockResumeThread` | 阻塞目标线程恢复 |
| `HookSelection` | `Mask` | 36 位 DbgkSys EPT Hook 掩码 |
| `UI` | `Theme` | `light` 或 `dark` |

### DebuggerList.ini

```ini
[配置]
Count=1
Debugger0=x64dbg&F:\Tools\x64dbg\x64dbg.exe
```

名称和路径使用 `&` 分隔。也兼容 `[DebuggerList]` 节。

### 符号

- 默认缓存根目录为 `C:\Symbols`。
- 优先读取 `SymbolTool\Config.ini`。
- 未配置时默认使用 Microsoft Symbol Server。
- PDB 按 PE RSDS 签名中的 GUID 和 Age 校验。
- 下载完成后再次校验，校验失败的缓存不会视为有效。

### 日志

| 日志 | 来源 |
|---|---|
| `Log\WinUI.log` | WinUI 启动、符号、配置和异常 |
| `UnrealDbgDll.log` | 后端加载、驱动、IOCTL 和初始化 |
| `Hook64.log` / `Hook.log` | 用户态 Hook 和断点透明化 |
| VTLoad 日志 | VT_Driver 加载和运行 |
| DbgkSys 日志 | 调试对象、EPT Hook 和事件路径 |

日志中心会联合识别 UTF-8、CP936/GBK、GB18030 和系统代码页，兼容新旧日志文件。

## 验证结果

本次 README 对应的源码结构在 2026-10-17 完成以下检查：

| 项目 | 结果 | 说明 |
|---|---|---|
| `Release|x64` 全解决方案构建 | PASS | 6 个工程全部构建成功 |
| WinUI 明亮主题冒烟测试 | PASS | `--ui-smoke-test --ui-theme=light` 返回 0 |
| WinUI 暗黑主题冒烟测试 | PASS | `--ui-smoke-test --ui-theme=dark` 返回 0 |
| UnrealDbgDll 导出 | PASS | `dumpbin /exports` 可见 5 个预期导出 |
| README 编码 | PASS | UTF-8 No BOM，无乱码替换字符 |
| VMX 硬件进入 | 未执行 | 未加载 VT_Driver 进入 VMX Root Mode |
| EPT Hook 运行验证 | 未执行 | 需要实际 VT 会话 |
| EPT 2 MB 拆分验证 | 未执行 | 需要目标物理页和 Hook 场景 |
| INVEPT/INVVPID 运行验证 | 未执行 | 需要多核地址转换场景 |
| CR3 Tracker 运行验证 | 未执行 | 需要进程切换与目标地址空间 |
| MTF/TF/#DB 运行验证 | 未执行 | 需要单步、断点和异常组合 |
| 蓝屏与恢复路径 | 未执行 | 需要可回滚测试环境 |
| Win10/Win11 全版本矩阵 | 未执行 | 不同 Build 仍需逐项验证 |
| P-core/E-core 迁移 | 未执行 | 需要混合架构实机 |
| x64dbg/x32dbg/Cheat Engine | 未执行 | UI 支持登记和启动，不等于兼容性验证 |

源码构建通过不代表硬件行为通过。真实硬件兼容性必须通过目标机器、目标 Windows Build 和实际调试流程确定。

## 兼容性与限制

### Intel

- 需要真实 Intel VT-x、EPT 和项目实际请求的 VMX 能力。
- 通过 CPUID、`IA32_FEATURE_CONTROL` 和 capability MSR 判断能力。
- 不应使用 CPU 型号白名单代替 capability discovery。
- 8 代至 14 代 Core、Core X、Core Ultra、Xeon E 和 Xeon Scalable 只是目标范围，不等于全部通过实际验证。
- P-core/E-core 需要验证每核 VMCS、VPID、EPTP、CR3、DR、MTF 和迁移。

### Windows

- WinUI 最低目标版本为 Windows 10 2004 Build 19041。
- DbgkSys 以 Build 22000 为 Win10/Win11 分界。
- 未公开内核结构、调试对象布局和函数偏移会随 Build 改变。
- Windows 10 22H2、Windows 11 21H2/22H2/23H2/24H2/25H2 和 26200 系列都需要独立验证。
- HVCI、VBS、Secure Boot、测试签名策略、内核隔离和第三方安全软件可能阻止驱动加载。

### 外部调试器

- x64dbg、x32dbg 和 Cheat Engine 的版本必须与目标架构匹配。
- UI 中的路径成功启动不代表断点、异常、上下文、模块和线程事件全部兼容。
- 目标进程的加固、反调试和多线程行为可能改变实际结果。

## 故障排查

### 无法加载后端

- 检查根目录和 `bin/` 中是否存在 `UnrealDbgDll.dll`。
- 检查 DLL 架构是否为 x64。
- 使用 `dumpbin /exports` 检查 5 个导出。
- 检查依赖 DLL 是否缺失。

### 驱动加载失败

- 确认以管理员身份运行。
- 确认驱动签名和测试签名策略。
- 检查 `AIHelper.dll`。
- 检查同名服务和旧驱动实例。
- 查看事件查看器、VTLoad 和 DbgkSys 日志。

### VT 初始化失败

- 检查 BIOS/UEFI 是否开启 VT-x。
- 检查 Hyper-V、VBS、内存完整性和其它 Hyper-V 类组件是否占用 VMX。
- 检查 CPU、VMX capability 和 EPT 能力。
- 检查驱动版本是否一致。
- 检查是否在支持嵌套虚拟化的环境中运行。

### 没有调试事件

- 确认核心 Hook 没有被全部关闭。
- 检查 `NtCreateDebugObject`、`NtDebugActiveProcess`、`NtWaitForDebugEvent` 和 `NtDebugContinue`。
- 检查 DbgkSys 版本与 Windows Build。
- 检查 Hook64 是否成功注入或加载。
- 检查日志中的 VMCALL、EPT violation、MTF、CR3 和注入记录。

### Hook 打开后崩溃

- 记录完整 mask，确认是否启用了当前平台不支持的位。
- 检查目标 Windows Build、驱动版本和 PDB/符号。
- 检查对应 Hook 是否依赖 `NtCreateDebugObject` 或 `ValidateHwnd` 的安装顺序。
- 保存 dump，重点检查 VMCS ownership、EPT 页表、MTF、DR、CR3 和跨核失效。

### 符号准备失败

- 检查 `C:\Symbols` 权限。
- 检查代理、网络和符号服务器配置。
- 检查 PDB 的 GUID/Age 是否与目标模块匹配。
- 不要复用校验失败的旧缓存。

## 开发约束

- `HookSelection.h` 的位值属于跨模块二进制契约，不得重排已有位。
- 新增 Hook 时，必须同步更新 `HOOK_SELECTION_ALL`、WinUI 描述、驱动安装逻辑和 README。
- 修改 IOCTL 或共享结构时，必须检查 WinUI、UnrealDbgDll、DbgkSysWin10、DbgkSysWin11 和 VT_Driver。
- 修改 EPT 时必须检查拆分、权限、缓存类型、INVEPT、MTF 和错误回滚。
- 修改 CR3 时必须检查 Guest CR3、VMCS、进程地址空间、翻译缓存和断点地址。
- 修改断点时必须检查 INT3、原始字节、RIP 语义、并发命中、恢复和重装。
- 修改 DR 时必须检查 DR0-DR7、local/global enable、RW/LEN、VMCS 调试状态和 CPU 迁移。
- 修改 MTF/TF 时必须检查 owner、pending state、冲突、清理和会话销毁。
- 修改进程/线程 Hook 时必须检查进程退出、线程退出、模块卸载和驱动卸载。
- 不得通过 CPU 白名单、Windows 版本白名单或强制单核来规避真实能力发现。
- 不得用无意义 NULL 判断、全局禁用功能或简单加锁掩盖 ownership、lifetime 和状态机错误。

## 源码编码

- C/C++ 源文件默认遵循 `.editorconfig`，使用 UTF-8 BOM。
- MASM 文件使用 UTF-8 No BOM。
- 本 README 使用 UTF-8 No BOM。
- 含中文的历史文件必须确认原代码页后再写回。
- 禁止用系统默认编码无确认覆盖中文文件。

## 安全与授权

本项目涉及内核驱动、硬件虚拟化、调试对象、EPT 和进程保护。请仅在拥有明确授权的环境中使用。

不得用于未授权访问、绕过第三方授权、规避安全产品、破坏生产系统或侵犯他人数据。README 中的“隐藏”“保护”“绕过”等词语仅描述源码技术路径，不构成对具体用途的授权。

## License

仓库根目录未提供项目级 `LICENSE` 文件。`Common/Ring0/SymbolicAccess/Phnt/LICENSE` 仅覆盖对应第三方代码。在获得明确授权前，不应默认复制、分发或用于闭源商业产品。
