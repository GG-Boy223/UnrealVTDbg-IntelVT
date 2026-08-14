<div align="center">

# UnrealVTDbg-New

**基于 Intel VT-x/EPT 的 Windows 调试研究框架**

*C++ 宿主、双架构调试桥、Win10/Win11 内核调试驱动与独立 VMM*

![C++](https://img.shields.io/badge/C%2B%2B-17%20%7C%2020-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Toolchain](https://img.shields.io/badge/Toolchain-VS2022%20%2B%20WDK-5C2D91?style=flat-square)

</div>

---

> [!NOTE]
> **仓库边界**
> 仓库包含 C++ 源码、项目文件、EPT 策略测试和必要资源。Cheat Engine、x64dbg/x32dbg、`AIHelper.dll`、正式签名证书及可部署驱动包不在仓库中。

> [!CAUTION]
> **内核实验风险**
> VMX、EPT、内核 Hook 和测试签名错误可能导致系统崩溃。请先在可恢复的测试机或虚拟机中验证，不要直接用于生产环境。

## 功能概览

| 功能 | 说明 |
|:-----|:-----|
| **C++ 宿主** | `UnrealDbgHostCore` 提供运行时状态机，`UnrealDbgHostCli` 提供诊断、初始化和调试器启动命令 |
| **EPT/VMM** | `VT_Driver` 管理 VMX、EPT identity map、4KB split、EPT Hook/watch、MTF 和失效操作 |
| **调试驱动** | `DbgkSysWin10` 与 `DbgkSysWin11` 实现调试对象、事件、断点和 EPT Hook 接入 |
| **调试桥** | `Hook.dll` 与 `Hook64.dll` 适配 x86/x64 用户态调试器 API |
| **加载器** | `Loader` 准备符号与资源，然后启动 C++ 宿主 |
| **策略测试** | 可移植测试覆盖 EPT/VPID 能力门禁、EPT 覆盖范围和 MTRR 重叠规则 |

## 运行链路

```text
虚幻调试器.exe
  └─ UnrealDbgHostCli.exe
      └─ UnrealDbgHostCore
          └─ UnrealDbgDll.dll
              ├─ AIHelper.dll
              ├─ VT_Driver.sys
              ├─ DbgkSysWin10.sys / DbgkSysWin11.sys
              └─ Hook.dll / Hook64.dll
```

## 核心 Interface

| Module | Interface | 说明 |
|:-------|:----------|:-----|
| **Host Core** | `Runtime::Inspect()` | 检查运行目录和依赖文件 |
| **Host Core** | `Runtime::Activate()` | 校验旧 ABI 并初始化驱动链；每个进程只允许尝试一次 |
| **Host Core** | `Runtime::Launch(path)` | 在 Active 状态启动并注入外部调试器 |
| **Host Core** | `Runtime::State()` | 返回 `Cold`、`Ready`、`Active` 或 `Faulted` |
| **Host CLI** | `PrintLog` | 供 `UnrealDbgDll.dll` 回调的无修饰宿主导出 |

## 目录结构

```text
UnrealVTDbg-New/
├── Common/                  # 共享协议、工具和 Ring0 支持
├── DbgkSysWin10/            # Windows 10 调试驱动
├── DbgkSysWin11/            # Windows 11 调试驱动
├── Hook/                    # x86/x64 用户态调试桥
├── Loader/                  # 符号准备与图形启动器
├── Tests/                   # 可移植策略测试
├── UnrealDbgDll/            # 用户态驱动初始化与调试器启动适配
├── UnrealDbgHostCore/       # C++ 宿主核心静态库
├── UnrealDbgHostCli/        # C++ 管理员控制台宿主
├── VT_Driver/               # VMX/EPT 驱动
├── _deps/res/               # Loader 必需资源
└── build.bat                # 统一 VS/WDK 构建入口
```

## 构建

### 环境

| 依赖 | 要求 |
|:-----|:-----|
| **Visual Studio** | Visual Studio 2022，v143 C++ 工具集 |
| **WDK/SDK** | Windows Driver Kit 与 Windows SDK `10.0.26100.0` |
| **架构** | x64 主机；Hook 同时生成 x64 与 Win32 |
| **权限** | 运行宿主和加载驱动时需要管理员权限 |

构建脚本通过 `vswhere.exe` 定位 Visual Studio。编译产物进入 `bin/x64/Release/`，中间文件进入 `obj/`。

```bat
:: EPT 策略测试
build.bat tests

:: 仅 VT 驱动
build.bat vt

:: 三个内核驱动
build.bat drivers

:: Hook.dll 与 Hook64.dll
build.bat hooks

:: C++ 宿主
build.bat host

:: C++ 宿主与 Loader
build.bat launcher

:: 验证 UnrealDbg.sln 的 Release|x64 配置
build.bat solution

:: 全部 Release 目标
build.bat
```

`Release|x64` 是当前受支持的统一配置；solution 中其余旧配置仅为历史兼容，尚未纳入新的宿主链验收。

驱动默认使用 `SignMode=Off` 完成可重复编译；实际部署必须另行完成符合目标系统策略的签名。

## C++ 宿主命令

```bat
UnrealDbgHostCli.exe doctor
UnrealDbgHostCli.exe init
UnrealDbgHostCli.exe launch "C:\Path\To\Debugger.exe"
UnrealDbgHostCli.exe status
```

无参数启动会进入交互模式。需要连续执行 `init`、`launch` 和 `status` 时应使用交互模式，因为状态属于当前宿主进程。

`doctor` 会检查：

- x64 架构与管理员身份
- `UnrealDbgDll.dll`
- `AIHelper.dll`
- `Hook.dll` 与 `Hook64.dll`
- `VT_Driver.sys`
- `DbgkSysWin10.sys` 与 `DbgkSysWin11.sys`
- `VMProtectSDK64.dll`

> [!IMPORTANT]
> `AIHelper.dll` 当前没有源码或受控二进制，因此干净克隆可以完成主要项目编译，但不能完成真实驱动加载与注入验收。

## 验证状态

| 检查 | 当前状态 |
|:-----|:---------|
| **EPT 策略测试** | Release x64，0 warning / 0 error |
| **VT_Driver** | Release x64，0 warning / 0 error |
| **DbgkSysWin10/11** | Release x64，0 warning / 0 error |
| **Hook32/64** | Release，0 warning / 0 error |
| **C++ Host + Loader** | Release x64，0 warning / 0 error |
| **真实加载/注入** | 受 `AIHelper.dll`、驱动签名和测试环境限制 |

---

<div align="center">

**Platform:** Windows x64 | **License:** 未声明

</div>
