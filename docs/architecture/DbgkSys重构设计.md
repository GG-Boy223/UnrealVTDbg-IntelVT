# DbgkSys 重构设计方案

> [!NOTE]
> **文档状态**
> 这是 2026-06-12 重构过程中的历史设计草案，用于理解拆分思路和风险边界。当前真实状态以 active 工程文件、构建脚本和 `Task-Status.md` 为准。

## 一、项目概述

DbgkSysWin10 和 DbgkSysWin11 是两个高度相似的内核驱动项目，分别支持 Windows 10 和 Windows 11 系统。当前存在大量重复代码，需要通过抽取公共代码库来提高可维护性。

## 二、当前差异分析

### 2.1 项目结构对比

#### DbgkSysWin10 目录结构
```
DbgkSysWin10/
├── DbgkApi/
│   ├── Api/              # 调试 API 实现（分散在多个文件）
│   │   ├── CreateDebugObject.cpp/h
│   │   ├── DebugActiveProcess.cpp/h
│   │   ├── DebugContinue.cpp/h
│   │   ├── DebugObjectCommon.cpp/h
│   │   ├── RemoveProcessDebug.cpp/h
│   │   ├── SetInformationDebugObject.cpp/h
│   │   └── WaitForDebugEvent.cpp/h
│   ├── DbgkApi.cpp/h
│   └── DbgkApi_Bridge.cpp
├── Driver.cpp           # 3500+ 行巨型文件，包含所有驱动逻辑
├── Globals.cpp/h
├── DebugBreak/
├── Hooks/EptHook/
├── Init/Symbolic/
├── Memory/
├── Process/
├── Protect/
├── List/
├── Log/
├── Encrypt/Blowfish/
├── Hvm/
├── Asm/
└── ntos/inc/            # 内核结构体偏移量定义
    └── 10.0.19041.4291/
```

#### DbgkSysWin11 目录结构（已重构）
```
DbgkSysWin11/
├── Core/                # 驱动核心逻辑（已模块化）
│   ├── DriverMain.cpp       # DriverEntry, Unload, 设备创建
│   ├── IoctlDispatch.cpp    # IOCTL 主分发器
│   ├── IoctlHandlers.cpp    # 所有 IOCTL 处理函数
│   ├── HookSetup.cpp        # Hook 安装函数
│   ├── DebugEvents.cpp      # 调试事件处理
│   └── ProcessThread.cpp    # 进程/线程回调
├── DbgkApi/
│   ├── DbgkApi.cpp/h    # 调试 API 实现（单文件）
├── Driver.cpp           # 桥接文件（16 行）
├── Globals.cpp/h
├── DebugBreak/
├── Hooks/EptHook/
├── Init/Symbolic/
├── Memory/
├── Process/
├── Protect/
├── List/
├── Log/
├── Encrypt/Blowfish/
├── Hvm/
├── Asm/
└── ntos/inc/
    └── 10.0.19041.4291/
```

### 2.2 文件统计

| 项目 | 总文件数 | .cpp 文件数 | .h 文件数 |
|------|---------|-----------|----------|
| DbgkSysWin10 | 75 | 约 20 | 约 55 |
| DbgkSysWin11 | 65 | 约 20 | 约 45 |

### 2.3 完全相同的代码

以下模块在两个项目中**完全相同**，可以直接抽取到公共库：

1. **Log/** - 日志系统
   - `log.cpp / log.h`
   
2. **List/** - 链表操作
   - `MyList.cpp / MyList.h`
   
3. **Memory/** - 内存读写
   - `ReadWrite.h`
   
4. **Process/** - 进程操作
   - `Process.h / process.h`（文件名大小写不同但内容相同）
   
5. **Protect/** - 保护回调
   - `Windows/BypassFindWnd.h`
   - `Process/`
   - `Thread/`
   - `Callbacks.h`
   
6. **Hvm/** - Hypervisor 通信
   - `AsmCallset.h`
   - `vmcall_reason.h`
   - `hypervisor_gateway.cpp/h`
   
7. **Asm/** - 汇编变量
   - `AsmVar.h`
   
8. **Init/Symbolic/** - 符号表初始化
   - `InitNtoskrnl.cpp/h`
   - `InitSymbolic.cpp/h`
   - `InitWin32kbase.cpp/h` / `InitWin32kbase.h`
   - `InitWin32kfull.cpp/h` / `InitWin32kfull.h`
   
9. **Encrypt/Blowfish/** - 加密模块
   - `Blowfish.cpp / Blowfish.h`
   
10. **ntos/inc/** - 内核类型定义（非偏移量部分）
    - `amd64.h`, `extypes.h`, `ke.h`, `ketypes.h`, `ki.h`
    - `mi.h`, `mmtypes.h`, `ntdbg.h`, `ntexapi.h`
    - `ntlpcapi.h`, `ntosdef.h`, `obtypes.h`
    - `peb_teb.h`, `pecoff.h`, `psp.h`, `pstypes.h`

### 2.4 只有偏移量不同的代码

以下代码逻辑相同，但使用不同的结构体偏移量：

1. **Hooks/EptHook/EptHook.cpp/h**
   - 依赖 `eprocess_offset::DebugPort` 等偏移量
   - 逻辑完全相同，仅偏移量不同
   
2. **DbgkApi/DbgkApi.cpp**
   - 使用 `eprocess_offset`, `ethread_offset`, `kthread_offset`
   - 核心逻辑相同，仅偏移量访问不同
   
3. **DebugBreak/DebugBreak.cpp/h**
   - 依赖进程/线程结构体偏移量
   - 逻辑相同
   
4. **Globals.cpp/h**
   - 全局变量定义
   - 使用偏移量访问内核结构体
   - 两个版本略有差异（需详细对比）

### 2.5 Win10/Win11 特有代码

#### Win10 特有
- `DbgkApi/Api/` 目录下的所有文件（已被 Win11 合并到单个文件）
- `DbgkApi/DbgkApi_Bridge.cpp`（桥接文件，Win11 已删除）
- `Driver.cpp` 3500+ 行巨型文件（Win11 已重构）

#### Win11 特有
- `Core/` 目录下的所有文件（重构后的模块化代码）
- `Driver.cpp` 桥接文件（16 行）

### 2.6 偏移量定义差异

`ntos/inc/10.0.19041.4291/ntkrnlmp.h` 文件定义了内核结构体偏移量，**Win10 和 Win11 的偏移量相同**（因为都基于同一 PDB）。

**重要发现：** 目前两个项目使用相同的偏移量文件（10.0.19041.4291），但为了支持不同 Win10/Win11 版本，未来需要参数化偏移量。

## 三、DbgkCommon 公共代码库设计

### 3.1 目录结构

```
DbgkCommon/                          # 公共代码库根目录
├── Core/                            # 核心抽象层
│   ├── OffsetConfig.h               # 偏移量配置接口
│   ├── VersionAdapter.h             # 版本适配器接口
│   └── CommonTypes.h                # 公共类型定义
├── DbgkApi/                         # 调试 API 实现
│   ├── DbgkApi.cpp
│   ├── DbgkApi.h
│   └── DbgkApiImpl.h                # 内部实现（使用偏移量宏）
├── DebugBreak/                      # 断点处理
│   ├── DebugBreak.cpp
│   └── DebugBreak.h
├── Hooks/                           # EPT Hook
│   └── EptHook/
│       ├── EptHook.cpp
│       └── EptHook.h
├── Init/                            # 初始化模块
│   └── Symbolic/
│       ├── InitNtoskrnl.cpp/h
│       ├── InitSymbolic.cpp/h
│       ├── InitWin32kbase.cpp/h
│       └── InitWin32kfull.cpp/h
├── Memory/                          # 内存操作
│   ├── ReadWrite.cpp
│   └── ReadWrite.h
├── Process/                         # 进程/线程操作
│   ├── Process.cpp
│   └── Process.h
├── Protect/                         # 保护回调
│   ├── Callbacks.h
│   ├── Windows/BypassFindWnd.h
│   ├── Process/
│   └── Thread/
├── List/                            # 链表操作
│   ├── MyList.cpp
│   └── MyList.h
├── Log/                             # 日志系统
│   ├── log.cpp
│   └── log.h
├── Encrypt/                         # 加密模块
│   └── Blowfish/
│       ├── Blowfish.cpp
│       └── Blowfish.h
├── Hvm/                             # Hypervisor 通信
│   ├── AsmCallset.h
│   ├── vmcall_reason.h
│   ├── hypervisor_gateway.cpp
│   └── hypervisor_gateway.h
├── Asm/                             # 汇编变量
│   └── AsmVar.h
├── Globals/                         # 全局变量（需参数化）
│   ├── GlobalsCommon.h              # 公共全局变量声明
│   └── GlobalsImpl.cpp              # 实现（使用偏移量宏）
└── ntos/                            # 内核类型定义
    └── inc/
        ├── Common/                  # 版本无关的类型定义
        │   ├── amd64.h
        │   ├── extypes.h
        │   ├── ke.h
        │   ├── ketypes.h
        │   ├── ki.h
        │   ├── mi.h
        │   ├── mmtypes.h
        │   ├── ntdbg.h
        │   ├── ntexapi.h
        │   ├── ntlpcapi.h
        │   ├── ntosdef.h
        │   ├── obtypes.h
        │   ├── peb_teb.h
        │   ├── pecoff.h
        │   ├── psp.h
        │   └── pstypes.h
        └── Offsets/                 # 版本相关的偏移量定义
            ├── Win10_19041/
            │   └── ntkrnlmp.h       # Win10 19041 偏移量
            ├── Win11_22000/
            │   └── ntkrnlmp.h       # Win11 22000 偏移量
            └── Win11_22621/
                └── ntkrnlmp.h       # Win11 22621 偏移量
```

### 3.2 偏移量参数化设计

#### 方案 1：编译期条件编译（推荐）

**优点：** 零运行时开销，类型安全
**缺点：** 需要为每个版本编译独立的二进制文件

```cpp
// DbgkCommon/Core/OffsetConfig.h
#pragma once

// 在项目编译时通过预处理器定义指定版本
// 例如：/D DBGK_TARGET_WIN10_19041 或 /D DBGK_TARGET_WIN11_22621

#if defined(DBGK_TARGET_WIN10_19041)
    #include "../ntos/inc/Offsets/Win10_19041/ntkrnlmp.h"
    #define DBGK_OS_NAME "Windows 10 (19041)"
#elif defined(DBGK_TARGET_WIN11_22000)
    #include "../ntos/inc/Offsets/Win11_22000/ntkrnlmp.h"
    #define DBGK_OS_NAME "Windows 11 (22000)"
#elif defined(DBGK_TARGET_WIN11_22621)
    #include "../ntos/inc/Offsets/Win11_22621/ntkrnlmp.h"
    #define DBGK_OS_NAME "Windows 11 (22621)"
#else
    #error "未定义目标操作系统版本！请指定 DBGK_TARGET_WINXX_XXXXX"
#endif

// 偏移量命名空间别名（统一接口）
namespace dbgk_offsets = eprocess_offset;  // 使用 ntkrnlmp.h 中定义的偏移量
```

**使用示例：**
```cpp
// DbgkCommon/DbgkApi/DbgkApi.cpp
#include "../Core/OffsetConfig.h"
#include "../ntos/inc/Common/pstypes.h"

PVOID DbgkCreateThread_CMP_Debugport_01()
{
    _EPROCESS* Process = (_EPROCESS*)PsGetCurrentProcess();
    
    // 使用统一的偏移量宏
    size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
    PDEBUG_OBJECT Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    
    return Port;
}
```

#### 方案 2：运行时动态偏移量表（备选）

**优点：** 单个二进制文件支持多版本
**缺点：** 运行时开销，需要版本检测

```cpp
// DbgkCommon/Core/OffsetConfig.h
#pragma once

struct DBGK_OFFSET_TABLE
{
    ULONG EProcessDebugPort;
    ULONG EProcessUniqueProcessId;
    ULONG EThreadCid;
    ULONG KThreadPreviousMode;
    ULONG KProcessDirectoryTableBase;
    // ... 更多偏移量
};

extern const DBGK_OFFSET_TABLE* g_DbgkOffsets;

NTSTATUS DbgkInitializeOffsets();  // 根据系统版本初始化偏移量表
```

**推荐使用方案 1**，因为：
1. 内核驱动对性能敏感，编译期解析零开销
2. 不同 Windows 版本通常需要独立签名和测试
3. 简化错误处理（编译期即可发现偏移量错误）

### 3.3 版本特化文件设计

对于需要版本特化的代码，使用以下模式：

```
DbgkSysWin10/
├── Platform/                        # 平台特化代码
│   ├── Win10Platform.cpp/h          # Win10 特有逻辑
│   └── VersionConfig.h              # 定义 DBGK_TARGET_WIN10_19041
├── Driver.cpp                       # 入口（调用 DbgkCommon）
└── [链接到 DbgkCommon]

DbgkSysWin11/
├── Platform/                        # 平台特化代码
│   ├── Win11Platform.cpp/h          # Win11 特有逻辑（如 PsTrustedAppsRuntimeDll）
│   └── VersionConfig.h              # 定义 DBGK_TARGET_WIN11_22621
├── Driver.cpp                       # 入口（调用 DbgkCommon）
└── [链接到 DbgkCommon]
```

**Win11 特有代码示例：**
```cpp
// DbgkSysWin11/Platform/Win11Platform.cpp
#include "Win11Platform.h"
#include "../../DbgkCommon/Core/OffsetConfig.h"

// Win11 25H2 特有：绕过 PsTrustedAppsRuntimeDll 检测
NTSTATUS Win11_BypassPsTrustedAppsRuntimeDll()
{
    // Win11 特有逻辑
    return STATUS_SUCCESS;
}

// Win11 平台初始化
NTSTATUS Win11_PlatformInit()
{
    NTSTATUS status = Win11_BypassPsTrustedAppsRuntimeDll();
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    
    // 调用公共初始化
    return DbgkCommon_Initialize();
}
```

## 四、公共代码抽取详细方案

### 4.1 完全相同代码的抽取

#### 步骤 1：创建 DbgkCommon 目录结构
```bash
mkdir -p DbgkCommon/{Core,DbgkApi,DebugBreak,Hooks/EptHook,Init/Symbolic,Memory,Process,Protect/{Windows,Process,Thread},List,Log,Encrypt/Blowfish,Hvm,Asm,Globals,ntos/inc/{Common,Offsets/{Win10_19041,Win11_22000,Win11_22621}}}
```

#### 步骤 2：复制完全相同的文件
```bash
# 从 DbgkSysWin11（已重构）复制到 DbgkCommon
cp DbgkSysWin11/Log/* DbgkCommon/Log/
cp DbgkSysWin11/List/* DbgkCommon/List/
cp DbgkSysWin11/Memory/* DbgkCommon/Memory/
cp DbgkSysWin11/Process/* DbgkCommon/Process/
cp -r DbgkSysWin11/Protect/* DbgkCommon/Protect/
cp -r DbgkSysWin11/Hvm/* DbgkCommon/Hvm/
cp DbgkSysWin11/Asm/* DbgkCommon/Asm/
cp -r DbgkSysWin11/Init/Symbolic/* DbgkCommon/Init/Symbolic/
cp -r DbgkSysWin11/Encrypt/Blowfish/* DbgkCommon/Encrypt/Blowfish/
```

#### 步骤 3：复制内核类型定义
```bash
# 版本无关的类型定义
cp DbgkSysWin11/ntos/inc/*.h DbgkCommon/ntos/inc/Common/

# 偏移量定义（需要为不同版本准备）
cp DbgkSysWin10/ntos/inc/10.0.19041.4291/ntkrnlmp.h DbgkCommon/ntos/inc/Offsets/Win10_19041/
cp DbgkSysWin11/ntos/inc/10.0.19041.4291/ntkrnlmp.h DbgkCommon/ntos/inc/Offsets/Win11_22621/
```

### 4.2 偏移量相关代码的抽取

#### 步骤 1：创建偏移量配置接口

创建 `DbgkCommon/Core/OffsetConfig.h`，提供统一的偏移量访问接口。

#### 步骤 2：抽取 DbgkApi.cpp

1. 复制 `DbgkSysWin11/DbgkApi/DbgkApi.cpp` 到 `DbgkCommon/DbgkApi/`
2. 修改包含路径，引入 `OffsetConfig.h`
3. 确保所有偏移量访问使用 `eprocess_offset::`, `ethread_offset::` 等命名空间

#### 步骤 3：抽取 EptHook.cpp

1. 复制 `DbgkSysWin11/Hooks/EptHook/EptHook.cpp` 到 `DbgkCommon/Hooks/EptHook/`
2. 修改包含路径
3. 验证偏移量使用正确

#### 步骤 4：抽取 DebugBreak.cpp

1. 复制 `DbgkSysWin11/DebugBreak/DebugBreak.cpp` 到 `DbgkCommon/DebugBreak/`
2. 修改包含路径
3. 确保断点处理逻辑使用正确的偏移量

#### 步骤 5：抽取 Globals.cpp

1. 对比 `DbgkSysWin10/Globals.cpp` 和 `DbgkSysWin11/Globals.cpp` 的差异
2. 将公共部分抽取到 `DbgkCommon/Globals/GlobalsCommon.h`
3. 将版本相关部分保留在各自项目的 `Platform/` 目录

### 4.3 Driver.cpp 的重构

#### Win10 Driver.cpp 重构方案

**当前状态：** 3500+ 行巨型文件，包含所有驱动逻辑

**重构方案：** 参考 Win11 的模块化结构

1. **创建 Core/ 目录**
   ```
   DbgkSysWin10/Core/
   ├── DriverMain.cpp       # DriverEntry, Unload, 设备创建/销毁
   ├── IoctlDispatch.cpp    # HandlerDispatchRoutin 主分发器
   ├── IoctlHandlers.cpp    # 所有 Handle_IOCTL_* 函数
   ├── HookSetup.cpp        # SetupHook_* 函数
   ├── DebugEvents.cpp      # Original_* 调试事件处理函数
   └── ProcessThread.cpp    # 进程/线程创建退出回调
   ```

2. **拆分步骤：**
   - 从 Driver.cpp 提取 `DriverEntry`, `Unload` 到 `DriverMain.cpp`
   - 提取 `HandlerDispatchRoutin` 到 `IoctlDispatch.cpp`
   - 提取所有 IOCTL 处理函数到 `IoctlHandlers.cpp`
   - 提取 `SetupHook_*` 函数到 `HookSetup.cpp`
   - 提取 `Original_*` 函数到 `DebugEvents.cpp`
   - 提取进程/线程回调到 `ProcessThread.cpp`

3. **保留 Driver.cpp 作为桥接文件**
   ```cpp
   // DbgkSysWin10/Driver.cpp
   #include "Driver.h"
   
   // 所有函数实现已迁移到 Core/ 目录的对应模块
   // 请参考各模块文件的具体实现
   ```

#### Win11 Driver.cpp 现状

**已完成重构，** 仅保留 16 行桥接代码，实际逻辑在 `Core/` 目录。

**可以直接复用 Win11 的模块化结构。**

### 4.4 DbgkApi 的统一

#### Win10 DbgkApi 结构
```
DbgkApi/
├── Api/
│   ├── CreateDebugObject.cpp/h
│   ├── DebugActiveProcess.cpp/h
│   ├── DebugContinue.cpp/h
│   ├── DebugObjectCommon.cpp/h
│   ├── RemoveProcessDebug.cpp/h
│   ├── SetInformationDebugObject.cpp/h
│   └── WaitForDebugEvent.cpp/h
├── DbgkApi.cpp/h
└── DbgkApi_Bridge.cpp
```

#### Win11 DbgkApi 结构（已合并）
```
DbgkApi/
├── DbgkApi.cpp          # 所有 API 实现合并到单个文件
└── DbgkApi.h
```

**重构方案：**
1. Win10 保持 `Api/` 目录结构（如果需要）
2. Win11 的单文件结构作为 DbgkCommon 的标准
3. Win10 可以选择：
   - 方案 A：保留多文件结构，链接到各自的 .cpp
   - 方案 B：合并到单文件，与 Win11 一致

**推荐方案 B**，因为：
- 简化 DbgkCommon 的维护
- 减少文件数量
- Win11 已验证单文件可行

## 五、迁移步骤

### 5.1 Phase 1：建立 DbgkCommon 基础（第 1 周）

**目标：** 创建 DbgkCommon 目录结构，抽取完全相同的代码

1. **创建目录结构**
   ```bash
   mkdir -p DbgkCommon/{Core,DbgkApi,DebugBreak,Hooks/EptHook,Init/Symbolic,Memory,Process,Protect,List,Log,Encrypt/Blowfish,Hvm,Asm,Globals,ntos/inc}
   ```

2. **抽取完全相同的代码**
   - 复制 Log, List, Memory, Process, Protect, Hvm, Asm, Init/Symbolic, Encrypt/Blowfish
   - 验证编译通过

3. **创建偏移量配置接口**
   - 编写 `Core/OffsetConfig.h`
   - 编写 `Core/CommonTypes.h`
   - 复制 ntos/inc 类型定义到 `ntos/inc/Common/`
   - 复制偏移量文件到 `ntos/inc/Offsets/`

4. **配置编译系统**
   - 修改 DbgkSysWin10 和 DbgkSysWin11 的项目文件
   - 添加 DbgkCommon 包含路径
   - 定义编译宏（DBGK_TARGET_WIN10_19041 / DBGK_TARGET_WIN11_22621）

**验证：** 两个项目能够链接 DbgkCommon 的代码并成功编译

### 5.2 Phase 2：抽取偏移量相关代码（第 2 周）

**目标：** 抽取 DbgkApi, EptHook, DebugBreak, Globals

1. **抽取 DbgkApi**
   - 复制 `DbgkSysWin11/DbgkApi/DbgkApi.cpp` 到 DbgkCommon
   - 修改包含路径
   - Win10 项目删除本地 DbgkApi，链接 DbgkCommon
   - Win11 项目删除本地 DbgkApi，链接 DbgkCommon

2. **抽取 EptHook**
   - 复制 `EptHook.cpp/h` 到 DbgkCommon
   - 修改包含路径
   - 验证两个项目都能正确使用

3. **抽取 DebugBreak**
   - 复制 `DebugBreak.cpp/h` 到 DbgkCommon
   - 修改包含路径
   - 验证断点功能正常

4. **抽取 Globals**
   - 对比两个版本的差异
   - 抽取公共部分到 DbgkCommon
   - 版本特化部分保留在各自项目

**验证：** 运行完整的功能测试，确保调试功能正常

### 5.3 Phase 3：重构 Win10 Driver.cpp（第 3 周）

**目标：** 将 Win10 的 3500 行 Driver.cpp 模块化

1. **创建 Core/ 目录**
   ```bash
   mkdir -p DbgkSysWin10/Core
   ```

2. **拆分 Driver.cpp**
   - 提取 DriverEntry, Unload → `Core/DriverMain.cpp`
   - 提取 IOCTL 分发 → `Core/IoctlDispatch.cpp`
   - 提取 IOCTL 处理函数 → `Core/IoctlHandlers.cpp`
   - 提取 Hook 安装 → `Core/HookSetup.cpp`
   - 提取调试事件 → `Core/DebugEvents.cpp`
   - 提取进程/线程回调 → `Core/ProcessThread.cpp`

3. **简化 Driver.cpp**
   - 保留为桥接文件（参考 Win11）

4. **验证编译和功能**
   - 编译 Win10 项目
   - 运行完整测试

**验证：** Win10 项目结构与 Win11 一致，代码清晰易维护

### 5.4 Phase 4：进一步抽取 Core 逻辑（第 4 周）

**目标：** 将 Core/ 目录的公共代码抽取到 DbgkCommon

1. **对比 Win10 和 Win11 的 Core/ 代码**
   - 分析 `DriverMain.cpp` 差异
   - 分析 `IoctlDispatch.cpp` 差异
   - 分析 `IoctlHandlers.cpp` 差异
   - 分析 `HookSetup.cpp` 差异
   - 分析 `DebugEvents.cpp` 差异
   - 分析 `ProcessThread.cpp` 差异

2. **抽取公共部分**
   - 将完全相同的函数抽取到 DbgkCommon/Core/
   - 将仅偏移量不同的函数参数化后抽取
   - 版本特化函数保留在各自项目的 Platform/

3. **创建平台特化接口**
   ```cpp
   // DbgkCommon/Core/PlatformInterface.h
   
   // 平台特化函数接口
   NTSTATUS Platform_Init();
   NTSTATUS Platform_Shutdown();
   NTSTATUS Platform_HandleSpecialCase();
   ```

4. **实现平台特化代码**
   - `DbgkSysWin10/Platform/Win10Platform.cpp`
   - `DbgkSysWin11/Platform/Win11Platform.cpp`

**验证：** 代码复用最大化，版本特化最小化

### 5.5 Phase 5：优化和文档（第 5 周）

**目标：** 清理遗留代码，完善文档

1. **清理死代码**
   - 检查并删除未使用的函数
   - 清理注释掉的代码
   - 统一代码风格（Allman 大括号）

2. **完善文档**
   - 编写 `DbgkCommon/README.md`
   - 更新各模块的注释
   - 编写迁移指南

3. **性能优化**
   - 检查偏移量访问效率
   - 优化热路径代码

4. **测试覆盖**
   - Win10 19041 完整测试
   - Win11 22621 完整测试
   - 回归测试

**验证：** 代码质量高，文档完善，测试通过

## 六、版本特化文件设计

### 6.1 Win10 特化

```
DbgkSysWin10/
├── Platform/
│   ├── VersionConfig.h      # 定义 DBGK_TARGET_WIN10_19041
│   ├── Win10Platform.cpp
│   └── Win10Platform.h
├── Core/                    # 模块化后的驱动逻辑（可能部分抽取到 DbgkCommon）
│   ├── DriverMain.cpp
│   ├── IoctlDispatch.cpp
│   ├── IoctlHandlers.cpp
│   ├── HookSetup.cpp
│   ├── DebugEvents.cpp
│   └── ProcessThread.cpp
├── Driver.cpp              # 桥接文件
├── Driver.h
└── [链接 DbgkCommon]
```

**Win10 特有逻辑（如果有）：**
- 旧版本 Windows 的兼容性处理
- 特定补丁版本的绕过

### 6.2 Win11 特化

```
DbgkSysWin11/
├── Platform/
│   ├── VersionConfig.h      # 定义 DBGK_TARGET_WIN11_22621
│   ├── Win11Platform.cpp    # Win11 特有逻辑
│   └── Win11Platform.h
├── Core/                    # 模块化驱动逻辑（可能部分抽取到 DbgkCommon）
│   ├── DriverMain.cpp
│   ├── IoctlDispatch.cpp
│   ├── IoctlHandlers.cpp
│   ├── HookSetup.cpp
│   ├── DebugEvents.cpp
│   └── ProcessThread.cpp
├── Driver.cpp              # 桥接文件（16 行）
├── Driver.h
└── [链接 DbgkCommon]
```

**Win11 特有逻辑：**
- `Platform/Win11Platform.cpp` 中实现 PsTrustedAppsRuntimeDll 绕过
- Win11 25H2 特有的安全检测绕过
- 新内核特性的支持

### 6.3 平台接口示例

```cpp
// DbgkCommon/Core/PlatformInterface.h
#pragma once

// 平台初始化接口
NTSTATUS Platform_InitializeDriver(PDRIVER_OBJECT DriverObject);

// 平台清理接口
NTSTATUS Platform_ShutdownDriver(PDRIVER_OBJECT DriverObject);

// 平台特定的 Hook 安装
NTSTATUS Platform_InstallSpecialHooks();

// 平台特定的安全检测绕过
NTSTATUS Platform_BypassSecurityChecks();
```

**Win10 实现：**
```cpp
// DbgkSysWin10/Platform/Win10Platform.cpp
#include "Win10Platform.h"
#include "../../DbgkCommon/Core/PlatformInterface.h"

NTSTATUS Platform_InitializeDriver(PDRIVER_OBJECT DriverObject)
{
    // Win10 特有初始化
    outLog("初始化 Win10 平台特化代码...");
    
    // 调用公共初始化
    return DbgkCommon_Initialize(DriverObject);
}

NTSTATUS Platform_BypassSecurityChecks()
{
    // Win10 没有 PsTrustedAppsRuntimeDll
    return STATUS_SUCCESS;
}
```

**Win11 实现：**
```cpp
// DbgkSysWin11/Platform/Win11Platform.cpp
#include "Win11Platform.h"
#include "../../DbgkCommon/Core/PlatformInterface.h"

NTSTATUS Platform_InitializeDriver(PDRIVER_OBJECT DriverObject)
{
    // Win11 特有初始化
    outLog("初始化 Win11 平台特化代码...");
    
    // Win11 特有：绕过 PsTrustedAppsRuntimeDll
    NTSTATUS status = Win11_BypassPsTrustedAppsRuntimeDll();
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    
    // 调用公共初始化
    return DbgkCommon_Initialize(DriverObject);
}

NTSTATUS Platform_BypassSecurityChecks()
{
    return Win11_BypassPsTrustedAppsRuntimeDll();
}

// Win11 特有实现
NTSTATUS Win11_BypassPsTrustedAppsRuntimeDll()
{
    outLog("绕过 Win11 PsTrustedAppsRuntimeDll 检测...");
    
    // Win11 25H2 特有逻辑
    // ... 具体实现 ...
    
    return STATUS_SUCCESS;
}
```

## 七、编译配置

### 7.1 预处理器定义

**DbgkSysWin10 项目配置：**
```
Configuration Properties > C/C++ > Preprocessor > Preprocessor Definitions
添加：DBGK_TARGET_WIN10_19041
```

**DbgkSysWin11 项目配置：**
```
Configuration Properties > C/C++ > Preprocessor > Preprocessor Definitions
添加：DBGK_TARGET_WIN11_22621
```

### 7.2 包含目录

**两个项目都添加：**
```
Configuration Properties > C/C++ > General > Additional Include Directories
添加：$(SolutionDir)DbgkCommon
```

### 7.3 链接库配置

**静态链接方案（推荐）：**

1. 将 DbgkCommon 编译为静态库 (.lib)
2. DbgkSysWin10 和 DbgkSysWin11 链接该静态库

**源代码引用方案（备选）：**

1. 在项目中直接添加 DbgkCommon 的 .cpp 文件
2. 通过包含路径访问头文件

**推荐静态链接方案**，因为：
- 便于模块化管理
- 加快编译速度（DbgkCommon 只编译一次）
- 清晰的依赖关系

## 八、测试策略

### 8.1 单元测试

**测试范围：**
- DbgkCommon 中每个公共函数
- 偏移量访问正确性
- 版本适配器功能

**测试方法：**
- 在 Win10 和 Win11 环境分别运行
- 验证偏移量读取的正确性
- 验证调试 API 功能

### 8.2 集成测试

**测试场景：**
1. 调试器连接目标进程
2. 设置断点（硬件断点、软件断点）
3. 单步执行
4. 读写内存
5. 获取寄存器
6. 处理异常事件

**测试环境：**
- Win10 19041.4291
- Win11 22621 (22H2)
- Win11 25H2（如果支持）

### 8.3 回归测试

**测试内容：**
- 所有已知的游戏反调试绕过
- 历史 bug 不复现
- 性能无明显下降

**测试方法：**
- 运行自动化测试脚本
- 对比重构前后的行为一致性

## 九、风险评估

### 9.1 技术风险

| 风险项 | 可能性 | 影响 | 缓解措施 |
|-------|-------|------|---------|
| 偏移量配置错误 | 中 | 高 | 编译期检查，完整测试 |
| 遗漏版本特化代码 | 中 | 高 | 详细对比两个项目差异 |
| 引入新 bug | 低 | 高 | 完整回归测试 |
| 性能下降 | 低 | 中 | 性能基准测试 |
| 编译配置错误 | 中 | 中 | 文档化编译步骤 |

### 9.2 项目风险

| 风险项 | 可能性 | 影响 | 缓解措施 |
|-------|-------|------|---------|
| 迁移时间超预期 | 中 | 中 | 分阶段迁移，逐步验证 |
| 破坏现有功能 | 低 | 高 | 保留原代码备份，充分测试 |
| 文档不完善 | 高 | 低 | 同步编写文档 |

### 9.3 维护风险

| 风险项 | 可能性 | 影响 | 缓解措施 |
|-------|-------|------|---------|
| 新版本 Windows 支持 | 高 | 中 | 设计灵活的偏移量系统 |
| 代码理解困难 | 中 | 中 | 完善注释和文档 |
| 多人协作冲突 | 低 | 低 | Git 分支管理 |

## 十、总结

### 10.1 预期收益

1. **代码复用率提升至 80%+**
   - 减少重复代码维护成本
   - 统一 bug 修复流程

2. **可维护性显著提升**
   - 模块化清晰
   - 版本差异明确
   - 新增 Windows 版本支持更容易

3. **代码质量提升**
   - 统一代码风格
   - 清理死代码
   - 完善文档

4. **开发效率提升**
   - bug 修复一次，两个版本同时受益
   - 新功能开发只需实现一次
   - 减少测试工作量

### 10.2 关键成功因素

1. **详细的差异分析**
   - 准确识别完全相同的代码
   - 正确处理偏移量差异
   - 明确版本特化需求

2. **合理的架构设计**
   - 偏移量参数化方案
   - 平台接口设计
   - 清晰的目录结构

3. **充分的测试**
   - 单元测试覆盖
   - 集成测试验证
   - 回归测试保障

4. **完善的文档**
   - 迁移指南
   - 编译配置说明
   - API 使用文档

### 10.3 下一步行动

1. **立即行动：** 创建 DbgkCommon 目录结构
2. **第 1 周：** 抽取完全相同的代码
3. **第 2 周：** 抽取偏移量相关代码
4. **第 3 周：** 重构 Win10 Driver.cpp
5. **第 4 周：** 进一步抽取 Core 逻辑
6. **第 5 周：** 优化、测试、文档

---

**文档版本：** 1.0  
**创建日期：** 2026-06-08  
**最后更新：** 2026-06-08  
**作者：** Claude (Kiro AI Assistant)
