<div align="center">

# UnrealVTDbg

**Windows x64-only VT-x hypervisor debugging codebase**

*VT-x driver, Dbgk drivers, debugger hook DLL, Delphi UI bridge, and build tooling*

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Toolchain](https://img.shields.io/badge/Toolchain-VS%202022%20%2B%20WDK-green?style=flat-square)
![Scope](https://img.shields.io/badge/Scope-x64--only-orange?style=flat-square)

</div>

---

> [!NOTE]
> **仓库定位**
> 这个仓库只维护 x64/Win64 源码、工程文件、构建脚本、文档，以及少量已纳入版本控制的必要源码侧资产。

## 模块概览

### C++ / Driver

| 模块 | 类型 | 职责 |
|:-----|:-----|:-----|
| **VT_Driver** | Kernel driver | VT-x/VMX hypervisor、VMM 初始化、VMExit、VMCall、EPT hook/watch。 |
| **DbgkSysWin10** | Kernel driver | Windows 10 调试对象、调试事件、断点、读写内存和 IOCTL 分发。 |
| **DbgkSysWin11** | Kernel driver | Windows 11 调试系统驱动，与 Win10 分支成对维护。 |
| **Hook** | User-mode DLL | 注入调试器进程，拦截调试 API，并与 Dbgk 驱动通信。 |
| **Loader** | User-mode EXE | x64 启动器源码，负责启动链路和符号流程。 |
| **UnrealDbgDll** | User-mode DLL | Delphi UI 到驱动层的桥接 DLL。 |
| **D-encryptiondll** | User-mode DLL | 文件加解密导出 DLL。 |
| **Common** | Shared code | 跨模块协议、ring0 工具、Detours、VMProtect SDK 声明、MD5、Blowfish、字符串工具。 |

### Delphi

| 模块 | 工程文件 | 职责 |
|:-----|:---------|:-----|
| **UnrealDbg** | `UnrealDbg/UnrealDbg.dproj` | Win64 UI 主程序源码。 |
| **D-encryption** | `D-encryption/D_encryption.dproj` | Delphi 加密工具源码。 |
| **CardRegistration** | `CardRegistration/卡密注册.dproj` | 卡密注册工具源码。 |
| **SymbolTool** | `SymbolTool/SymbolTool.dproj` | 符号工具源码。 |

## 模块分组

### VT_Driver

- `Driver/`: driver entry, lifecycle, guest commands.
- `Vmm/`: VMM context, VMX entry, VCPU setup.
- `VmExit/`: VMExit dispatch and handlers.
- `VmCall/`: guest-to-hypervisor command dispatch.
- `Ept/`: EPT page tables, hook, watch, sync.
- `HypervisorRoutines/`: VMX/VMCS helper routines.
- `PoolManager/`: kernel pool allocation helpers.

### DbgkSysWin10 / DbgkSysWin11

- `Core/`: driver entry, device setup, IOCTL dispatch.
- `DbgkApi/`: debug object lifecycle and debug event API.
- `DebugBreak/`: software and hardware breakpoint support.
- `Hvm/`: hypervisor gateway and EPT hook bridge.
- `Process/`: process and thread debug state.
- `Init/Symbolic/`: kernel symbol and offset initialization.

### Hook / Loader / UnrealDbgDll

- `Hook/DllMain/`: DLL entry and hook lifecycle.
- `Hook/HookCallSet/Hooks/`: intercepted debug API implementations.
- `Hook/Channels/`: driver communication channels.
- `Loader/WinMain/`: launcher entry and process flow.
- `Loader/Symbols/`: kernel symbol download flow.
- `UnrealDbgDll/Core/`: initialization, symbols, version helpers.
- `UnrealDbgDll/Init/`: driver service path and startup handling.
- `UnrealDbgDll/Process/Launch/`: debugger process launch and hook injection.
- `UnrealDbgDll/Interface/`: IOCTL-facing exported helpers.

## 核心入口

### Driver

| 模块 | API | 文件 |
|:-----|:----|:-----|
| **VT_Driver** | `DriverEntry` | `VT_Driver/Driver/DriverLifecycle.cpp` |
| **VT_Driver** | `vmm_init` | `VT_Driver/Vmm/VmmEntry.cpp` |
| **VT_Driver** | `vmexit_handler` | `VT_Driver/VmExit/VmExitDispatcher.cpp` |
| **VT_Driver** | `vmexit_vmcall_handler` | `VT_Driver/VmCall/vmcall_handler.cpp` |
| **DbgkSysWin10/11** | `DriverEntry` | `Core/DriverMain.cpp` |
| **DbgkSysWin10/11** | `HandlerDispatchRoutin` | `Core/IoctlDispatch.cpp` |
| **DbgkSysWin10/11** | `DispatchClientIoctl` | `Core/IoctlDispatchClient.cpp` |
| **DbgkSysWin10/11** | `DispatchBreakpointIoctl` | `Core/IoctlDispatchBreakpoints.cpp` |
| **DbgkSysWin10/11** | `InitSymbolsTable` | `Init/Symbolic/InitSymbolic.cpp` |
| **DbgkSysWin10/11** | `DbgkInitialize` | `DbgkApi/Api/DebugObjectLifecycle.cpp` |

### User Mode

| 模块 | API | 文件 |
|:-----|:----|:-----|
| **Hook** | `SetupHook` / `UnHook` | `Hook/DllMain/Core/DllMainHookSetup.cpp` |
| **Loader** | `WinMain` | `Loader/WinMain.cpp` |
| **UnrealDbgDll** | `Initialize_F3` / `Initialize` | `UnrealDbgDll/Core/SymbolInitialization.cpp` |
| **UnrealDbgDll** | `StartProcess` | `UnrealDbgDll/Process/Launch/StartProcess.cpp` |
| **UnrealDbgDll** | `GetFileVersion` | `UnrealDbgDll/Core/FileVersion.cpp` |
| **UnrealDbgDll** | `TL_BlockGameResumeThread` | `UnrealDbgDll/Interface/Interface.cpp` |
| **D-encryptiondll** | `EncryptDataToFile` / `DecryptDataFromFile` | `D-encryptiondll/D-encryption/dllmain.cpp` |

## 目录结构

```
UnrealVTDbg/
├── Common/                  shared code and protocol definitions
├── DbgkSysWin10/            Windows 10 debug-system driver
├── DbgkSysWin11/            Windows 11 debug-system driver
├── Hook/                    x64 injected hook DLL
├── Loader/                  x64 launcher source
├── UnrealDbg/               Delphi Win64 UI source
├── UnrealDbgDll/            C++ bridge DLL
├── VT_Driver/               VT-x hypervisor driver
├── D-encryptiondll/         x64 encryption DLL
├── D-encryption/            Delphi Win64 encryption tool source
├── CardRegistration/        Delphi Win64 card-registration tool source
├── SymbolTool/              Delphi Win64 symbol tool source
├── docs/                    documentation index and architecture notes
├── scripts/                 build and release helper scripts
└── Task-Status.md           current task ledger
```

## 构建

> [!NOTE]
> C++/driver 构建使用 Visual Studio 2022 + WDK；Delphi 工程需要 dcc64 环境。

```bat
build_vt_driver_release_x64.bat
build_dbgk_win10_release_x64.bat
build_dbgk_win11_release_x64.bat
build_hook_release_x64.bat
build_loader_release_x64.bat
build_d_encryptiondll_release_x64.bat
build_unreal_dbgdll_release_x64.bat
```

| 输出 | 构建入口 |
|:-----|:---------|
| **VT_Driver.sys** | `build_vt_driver_release_x64.bat` |
| **DbgkSysWin10.sys** | `build_dbgk_win10_release_x64.bat` |
| **DbgkSysWin11.sys** | `build_dbgk_win11_release_x64.bat` |
| **Hook64.dll** | `build_hook_release_x64.bat` |
| **虚幻调试器.exe** | `build_loader_release_x64.bat` |
| **D-encryption.dll** | `build_d_encryptiondll_release_x64.bat` |
| **UnrealDbgDll.dll** | `build_unreal_dbgdll_release_x64.bat` |

Delphi Win64 工程清单和脚本边界见 [`scripts/README.md`](scripts/README.md)。

## 文档

| 文档 | 说明 |
|:-----|:-----|
| `Task-Status.md` | 当前任务、完成项和高价值发现。 |
| `docs/README.md` | 文档索引。 |
| `scripts/README.md` | 构建、发布和 legacy 脚本索引。 |
| `CLAUDE.md` | 工程协作规则和约束。 |

---

<div align="center">

**Platform:** Windows x64 | **Toolchain:** VS 2022 + WDK + Delphi Win64

</div>
