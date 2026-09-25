# UnrealVTDbg - 基于 Intel VT-x 的内核级调试系统

## 项目概述

UnrealVTDbg 是一个基于 Intel VT-x（Virtualization Technology）硬件虚拟化技术的内核级调试系统，通过构建 Type-1 Hypervisor 实现对 Windows 10/11 x64 操作系统的透明监控与调试能力。项目采用 Delphi + C++ 混合架构，在 VMX Root Mode 运行虚拟化监控层，利用 EPT（Extended Page Tables）实现隐蔽内存钩子与硬件断点虚拟化，并重建了完整的 Windows 调试对象（DebugObject）机制以绕过反调试检测。

### 核心特性

- **硬件虚拟化隔离**：基于 Intel VMX 技术构建 Type-1 Hypervisor，运行在 VMX Root Mode（-1环），对 Guest OS（Windows）完全透明，通过 VMLAUNCH/VMRESUME 管理虚拟机生命周期
- **EPT 内存虚拟化**：利用扩展页表（Extended Page Tables）实现二级地址转换（GVA→GPA→HPA），提供细粒度的页级内存访问控制（R/W/X权限独立管理）与隐蔽 Hook 能力
- **自建调试子系统**：完整重建 Windows 调试对象（DebugObject）机制，实现 NtDebugActiveProcess、NtWaitForDebugEvent、NtDebugContinue 等调试 API，绕过内核反调试检测
- **硬件断点虚拟化**：通过 EPT Violation 事件模拟硬件断点，突破 DR0-DR7 调试寄存器数量限制（最多支持 512 个监控点），支持读/写/执行三种访问类型监控
- **多核同步架构**：支持多处理器并发调试，每个逻辑 CPU 独立维护 VCPU 上下文与 EPT 状态，通过 DPC 广播实现跨核同步操作，保障 EPT 页表一致性
- **隐形断点机制**：基于 EPT 伪页（Fake Page）技术实现 INT3 软件断点隐藏，原始代码页与修改后的伪页通过 EPT 权限切换动态交换，对目标进程不可见
- **VMCALL 通信接口**：提供 25 条 VMCALL 指令集，用户态/内核态通过 VMCALL 与 Hypervisor 通信，支持 Hook 管理、断点操作、EPT 控制、TLB 刷新等功能

### 技术栈

| 层次 | 技术组件 | 说明 |
|------|----------|------|
| 用户界面 | Delphi 10.x | 调试器主界面、进程/线程管理、断点可视化、配置管理 |
| 用户态桥接 | C++ DLL (UnrealDbgDll, Hook) | 符号表初始化、IOCTL 通信封装、网络验证、用户态钩子 |
| 内核驱动 | C++ WDK 10.0.26100 | VT_Driver（VMX 虚拟化引擎）、DbgkSysWin10/Win11（调试子系统驱动） |
| 虚拟化层 | Intel VT-x, EPT, VMCS | VMX Root Mode、扩展页表、VMCS 配置、VM-Exit 处理、INVEPT/INVVPID |
| 辅助模块 | Loader, SymbolTool | 驱动加载器、PDB 符号解析工具 |

### 项目规模

- **总代码量**：832 个 C/C++ 源文件 + 15 个汇编文件，超过 15 万行代码
- **核心模块**：6 个 C++ 驱动项目（VT_Driver、DbgkSysWin10、DbgkSysWin11、Hook、UnrealDbgDll、Loader）+ 4 个 Delphi 项目（UnrealDbg、CardRegistration、D-encryption、SymbolTool）
- **驱动组件**：VT_Driver（虚拟化引擎，约 5 万行）、DbgkSysWin10/Win11（调试子系统，各约 2 万行）

---

## 架构设计

### 系统分层架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          Ring 3 - 用户态                                  │
├──────────────────────────────────────────────────────────────────────────┤
│  UnrealDbg (Delphi)              │  UnrealDbgDll / Hook (C++)            │
│  - 调试器主界面与交互             │  - PDB 符号表初始化与解析             │
│  - 进程/线程/模块管理             │  - IOCTL 通信封装与参数传递           │
│  - 断点可视化与状态展示           │  - 网络卡密验证（Blowfish 加密）      │
│  - 调试事件回调处理               │  - 用户态 Detours Hook 支持           │
└──────────────────────────────────────────────────────────────────────────┘
                    ↓ DeviceIoControl (IOCTL 通信)
┌──────────────────────────────────────────────────────────────────────────┐
│                          Ring 0 - 内核态                                  │
├──────────────────────────────────────────────────────────────────────────┤
│  DbgkSysWin10/Win11              │  VT_Driver (VMX 虚拟化引擎)           │
│  - 调试对象管理（DebugObject）    │  - VMM 全局上下文分配与管理           │
│  - NtDebugActiveProcess 实现      │  - VCPU 多核初始化（IO/MSR Bitmap）   │
│  - NtWaitForDebugEvent 实现       │  - VMCS 配置（Pin/Primary/Secondary） │
│  - NtDebugContinue 实现           │  - Host 页表构建（直接映射全物理内存）│
│  - 调试事件队列（CREATE/EXIT/DLL）│  - MTRR 缓存类型映射构建              │
│  - EPT Hook 内核函数              │  - Pool Manager 预分配内存池          │
│  - VMCALL 广播接口（DPC）         │  - VM-Exit 分发与处理引擎             │
│  - 隐形断点管理（伪页缓冲读写）   │  - EPT 页表动态构建与拆分（2MB→4KB）  │
│                                   │  - VMCALL 指令分发与参数验证          │
└──────────────────────────────────────────────────────────────────────────┘
                    ↓ VMLAUNCH (首次进入) / VMRESUME (恢复执行)
┌──────────────────────────────────────────────────────────────────────────┐
│                    VMX Root Mode (Hypervisor Layer)                      │
├──────────────────────────────────────────────────────────────────────────┤
│  VM-Exit 处理                    │  EPT 管理                              │
│  - EPT Violation 处理            │  - EPT PML4/PDPT/PD/PT 四级页表        │
│  - CPUID/MSR/IO 拦截             │  - 2MB 大页动态拆分（Split）           │
│  - VMCALL 指令处理               │  - 伪页内存管理（Fake Page）           │
│  - MTF 单步执行控制              │  - 页表引用计数与合并回收              │
│  - CR3 切换监控                  │  - INVEPT TLB 刷新（单上下文/全局）    │
│  - 异常/中断注入                 │  - 页权限动态切换（原页/伪页）         │
│                                   │                                        │
│  Hook 引擎                       │  断点虚拟化                            │
│  - CC Hook (INT3 断点)           │  - 读/写/执行监控（EPTW_*）           │
│  - INT1 Hook (#DB 调试异常)      │  - EPT Watch 列表（512 槽位）          │
│  - VMCALL Hook (RIP 重定向)      │  - 软件断点隐藏（原字节恢复）         │
│  - 函数 Hook (Trampoline)        │  - 断点分组管理（GroupId）             │
│  - 热引用机制（防并发卸载）      │  - MTF 配合权限恢复                    │
└──────────────────────────────────────────────────────────────────────────┘
                    ↓ EPT 地址转换 (GPA → HPA)
┌──────────────────────────────────────────────────────────────────────────┐
│                    VMX Non-Root Mode (Guest OS)                          │
│                         Windows 10 / Windows 11                          │
│  - 操作系统内核（ntoskrnl.exe）、驱动、用户进程正常执行                   │
│  - 访问被监控内存/执行 VMCALL 指令时触发 VM-Exit 陷入 Hypervisor         │
└──────────────────────────────────────────────────────────────────────────┘
```

### VMCALL 通信机制

用户态通过 IOCTL 发送调试请求 → 内核驱动解析参数 → 执行 VMCALL 指令（触发 VM-Exit） → VMX Root Mode 处理命令 → 返回结果到 RAX 寄存器 → 内核驱动返回用户态。

**VMCALL 指令识别**：
- 32 位标识符：`VMCALL_IDENTIFIER2 = 0x66666666U`（EAX 寄存器）
- 64 位标识符：`VMCALL_IDENTIFIER = 0xBF5587567C4C830FULL`（RAX 寄存器）

**VMCALL 命令集**（共 25 条指令，定义在 `Common/Shared/VmcallReason.h`）：

| 编号 | 指令名称 | 功能描述 |
|------|----------|----------|
| 0 | `VMCALL_TEST` | 测试 VMCALL 通信是否正常 |
| 1 | `VMCALL_VMXOFF` | 退出 VMX 模式，恢复正常执行 |
| 2 | `VMCALL_EPT_CC_HOOK` | 基于 INT3 (0xCC) 的 EPT Hook |
| 3 | `VMCALL_EPT_INT1_HOOK` | 基于 INT1 (#DB) 的 EPT Hook |
| 4 | `VMCALL_EPT_RIP_HOOK` | 基于 VMCALL 指令重定向的 RIP Hook |
| 5 | `VMCALL_EPT_HOOK_FUNCTION` | 通用函数 EPT Hook（生成 Trampoline） |
| 6 | `VMCALL_EPT_UNHOOK_FUNCTION` | 卸载指定函数 Hook 或全部 Hook |
| 7 | `VMCALL_INVEPT_CONTEXT` | 刷新 EPT TLB（单上下文或全局） |
| 8 | `VMCALL_DUMP_POOL_MANAGER` | 输出内存池管理器调试信息 |
| 9 | `VMCALL_DUMP_VMCS_STATE` | 输出当前 VMCS 状态信息 |
| 10 | `VMCALL_HIDE_HV_PRESENCE` | 隐藏 Hypervisor 存在（CPUID 隐藏） |
| 11 | `VMCALL_UNHIDE_HV_PRESENCE` | 显示 Hypervisor 存在 |
| 12 | `VMCALL_HIDE_SOFTWARE_BREAKPOINT` | 设置隐形 INT3 软件断点 |
| 13 | `VMCALL_READ_SOFTWARE_BREAKPOINT` | 读取断点位置原始字节 |
| 14 | `VMCALL_READ_EPT_FAKE_PAGE_MEMORY` | 读取 EPT 伪页内存内容 |
| 15 | `VMCALL_WATCH_WRITES` | 监控内存写入操作（EPTW_WRITE） |
| 16 | `VMCALL_WATCH_READS` | 监控内存读/写操作（EPTW_READWRITE） |
| 17 | `VMCALL_WATCH_EXECUTES` | 监控内存执行操作（EPTW_EXECUTE） |
| 18 | `VMCALL_WATCH_DELETE` | 删除指定内存监控点 |
| 19 | `VMCALL_GET_BREAKPOINT` | 获取并消费断点触发信息 |
| 20 | `VMCALL_INIT_OFFSET` | 初始化内核结构偏移（符号解析） |
| 21 | `VMCALL_CLEAR_BREAKPOINT_STATE` | 清除断点状态信息 |
| 22 | `VMCALL_SYNC_CR3_EXIT_POLICY` | 同步 CR3 监控策略（进程切换跟踪） |
| 23 | `VMCALL_SET_SOFTWARE_BREAKPOINT_BUFFER` | 写入伪页缓冲（隐形断点原字节） |
| 24 | `VMCALL_GET_SOFTWARE_BREAKPOINT_BUFFER` | 读取伪页缓冲（隐形断点原字节） |
| 25 | `VMCALL_PEEK_BREAKPOINT` | 非破坏性读取断点触发信息 |

**跨核同步机制**（DPC 广播）：
- 驱动通过 `KeGenericCallDpc` 向所有 CPU 广播 VMCALL 请求
- 每个核心独立执行 VMCALL 并返回状态码
- 驱动统计成功执行的核心数，与总核心数比较验证操作是否全局生效

---

## 核心模块详解

### 一、VT_Driver - VMX 虚拟化引擎

**功能定位**：项目的核心驱动，负责 Intel VT-x 虚拟化功能的初始化、VMCS 配置、VM-Exit 事件处理、EPT 页表管理、VMCALL 指令分发等。

**目录结构**：
```
VT_Driver/
├── Driver.cpp/h              # 驱动入口（DriverEntry/Unload）
├── Vmm/                      # 虚拟机监控器管理
│   ├── VmmContext.cpp        # VMM 全局上下文分配与释放
│   ├── VmmVcpu.cpp           # VCPU 初始化（IO/MSR Bitmap、EPT State）
│   └── VmmInit.cpp           # VMM 启动与多核初始化
├── Vmcs/                     # VMCS 配置与管理
│   ├── VmcsFill.cpp          # 填充 VMCS 控制字段（Pin/Primary/Secondary/Exit/Entry）
│   ├── VmcsControls.cpp      # 控制位调整（根据硬件能力调整请求值）
│   └── VmcsApi.cpp           # VMREAD/VMWRITE 封装接口
├── VmExit/                   # VM-Exit 事件处理
│   ├── VmExitDispatch.cpp    # VM-Exit 分发器（根据退出原因分发）
│   ├── VmExitEpt.cpp         # EPT Violation/Misconfiguration 处理
│   ├── VmExitInstruction.cpp # CPUID/VMCALL/XSETBV 等指令处理
│   ├── VmExitMsr.cpp         # MSR 读写拦截处理
│   └── VmExitTelemetry.cpp   # VM-Exit 统计遥测
├── VmCall/                   # VMCALL 指令处理
│   └── VmcallDispatch.cpp    # VMCALL 分发与参数验证
├── Ept/                      # EPT 内存虚拟化
│   ├── EptBuild.cpp          # EPT 页表初始化与构建
│   ├── EptCommon.cpp         # EPT 通用函数（页引用、函数引用）
│   ├── EptSync.cpp           # EPT 页表同步与 TLB 刷新
│   ├── Hook/                 # EPT Hook 实现
│   │   ├── Lifecycle/        # Hook 生命周期管理
│   │   │   ├── EptHookDefault.cpp   # 通用函数 Hook
│   │   │   ├── EptHookCc.cpp        # INT3 Hook
│   │   │   ├── EptHookInt1.cpp      # INT1 Hook
│   │   │   └── EptHookVmcall.cpp    # VMCALL Hook
│   │   ├── Instruction/      # Hook 指令处理
│   │   │   ├── EptHookInstruction.cpp       # Trampoline 生成
│   │   │   └── EptHookFakePageMemory.cpp    # 伪页内存读写
│   │   └── Cleanup/          # Hook 清理
│   │       └── EptHookCleanup.cpp   # Unhook 与资源释放
│   └── Watch/                # EPT 断点监控
│       ├── EptWatchPageInstall.cpp  # 监控页安装
│       ├── EptWatchBreakpoints.cpp  # 断点命中处理
│       └── EptWatchDeactivate.cpp   # 监控点停用
├── Cr3/                      # CR3 进程切换跟踪
│   └── Cr3Tracker.cpp        # CR3 切换监控与策略同步
├── ASM/                      # 汇编代码
│   ├── vm-launch.asm         # VMLAUNCH 汇编实现
│   ├── vm-exit.asm           # VM-Exit 入口汇编
│   ├── AsmCallset.asm        # VMCALL/VMREAD/VMWRITE 汇编包装
│   ├── exception-routines.asm # Host 异常处理程序
│   └── interrupt-handlers.asm # Host 中断处理程序
├── Globals/                  # 全局变量与数据结构
│   ├── GlobalsVmm.h          # VMM 全局上下文结构（g_vmm_context）
│   └── GlobalsEptHookTypes.h # EPT Hook 类型定义
└── poolmanager.cpp/h         # 预分配内存池管理器
```

**关键流程**：

1. **驱动初始化流程（DriverEntry）**：
   ```
   DriverEntry
   ├── InitNtoskrnlSymbolsTable()       # 初始化 ntoskrnl.exe 符号表
   ├── hv::virtualization_support()     # 检测 CPU 是否支持 VT-x
   ├── hv::InitGlobalVariables()        # 初始化全局变量
   └── vmm_init()                       # 启动 VMM
       ├── allocate_vmm_context()       # 分配 VMM 全局上下文
       │   ├── 分配 VCPU 数组（每核一个）
       │   ├── ept::build_mtrr_map()    # 构建 MTRR 缓存类型映射
       │   ├── pool_manager::initialize() # 初始化预分配内存池
       │   ├── init_vcpu() (每个核心)   # 初始化 VCPU 资源
       │   │   ├── 分配 IO Bitmap A/B
       │   │   ├── 分配 EPT State 结构
       │   │   └── ept::initialize()    # 初始化 EPT 页表
       │   └── create_host_page_tables() # 创建 Host 页表
       └── 多核并行执行 VMLAUNCH          # 每个核心进入 VMX 模式
   ```

2. **EPT Hook 工作原理**：
   - **初始状态**：目标页 EPT 权限为 R=1, W=1, X=1（可读写执行）
   - **设置 Hook**：
     1. 复制原始页内容到伪页（Fake Page）
     2. 在伪页中修改目标函数（INT3/跳转指令/VMCALL）
     3. 修改 EPT 条目：原页设为 R=1, W=1, X=0（不可执行）
     4. 保存伪页条目：R=0, W=0, X=1（仅可执行）
   - **触发 Hook**：
     - Guest 执行目标地址 → EPT Violation (Execute) → 切换到伪页 → 执行修改后的代码
     - Guest 读/写目标地址 → EPT Violation (Read/Write) → 切换到原页 → 读取原始数据
   - **MTF 单步恢复**：访问后设置 MTF（Monitor Trap Flag），下一条指令执行完毕后恢复 EPT 权限

3. **VM-Exit 处理流程**：
   ```
   Guest 执行特权操作 → CPU 触发 VM-Exit → 保存 Guest 状态到 VMCS
   → vm-exit.asm (汇编入口)
   → vmexit_handler() (C++ 处理器)
   → dispatch_vm_exit() (分发器)
   ├── EPT Violation → vmexit_ept_violation_handler()
   ├── CPUID → vmexit_cpuid_handler()
   ├── VMCALL → vmexit_vmcall_handler() → VmcallDispatch::Execute()
   ├── MSR 读写 → vmexit_msr_handler()
   └── 其他原因 → 对应处理函数
   → 恢复 Guest 状态 → VMRESUME 返回 Guest
   ```


### 二、DbgkSysWin10 / DbgkSysWin11 - 调试子系统驱动

**功能定位**：针对 Windows 10/11 分别实现的内核调试子系统，完整重建 Windows 调试对象（DebugObject）机制，提供自建的调试 API，绕过游戏驱动的反调试检测。

**目录结构**：
```
DbgkSysWin10/ (DbgkSysWin11 结构相同)
├── Driver.cpp/h              # 驱动入口
├── DbgkApi/                  # 调试 API 实现
│   └── Api/
│       ├── DebugActiveProcess.cpp        # 附加调试进程
│       ├── DebugActiveProcessAttach.cpp  # DbgkpSetProcessDebugObject 流程
│       ├── DebugActiveProcessEntry.cpp   # NtDebugActiveProcess 入口
│       ├── DebugObjectCommon.cpp         # 调试对象通用函数
│       ├── DebugContinue.cpp             # NtDebugContinue 实现
│       ├── WaitForDebugEvent.cpp         # NtWaitForDebugEvent 实现
│       ├── DebugImageEvents.cpp          # DLL 加载/卸载事件
│       ├── DebugObjectEvents.cpp         # 调试对象事件管理
│       └── SetInformationDebugObject.cpp # 设置调试对象信息
├── Core/                     # 核心功能
│   ├── IoctlDispatch.cpp     # IOCTL 分发器
│   ├── IoctlDispatchBreakpoints.cpp  # 断点相关 IOCTL
│   ├── IoctlDispatchClient.cpp       # 客户端通信 IOCTL
│   ├── IoctlHandlers/
│   │   ├── IoctlUserData.cpp         # 用户数据传递
│   │   └── ProtectListHandlers.cpp   # 保护列表管理
│   └── DebugEvents/
│       ├── ThreadCreationEvents.cpp  # 线程创建/退出事件
│       └── MemoryAndAttachEvents.cpp # 内存/附加事件
├── Hooks/                    # EPT Hook 管理
│   └── EptHook/
│       └── Core/
│           ├── EptHookNtosDebug.cpp      # Hook ntoskrnl.exe 调试函数
│           ├── EptHookThreadProcess.cpp  # Hook 进程/线程函数
│           ├── EptHookWin32k.cpp         # Hook win32k.sys 函数
│           └── EptHookMemoryObject.cpp   # Hook 内存对象函数
├── Hvm/                      # Hypervisor 网关接口
│   ├── HypervisorGatewayApi.cpp          # VMCALL 封装接口
│   ├── HypervisorGatewayBroadcast.cpp    # DPC 广播实现
│   └── HypervisorGatewayInternal.h       # 内部数据结构
├── Globals/                  # 全局变量与配置
│   └── Globals.cpp           # 全局状态管理
├── Encrypt/                  # 加密模块
│   └── Blowfish/             # Blowfish 加密算法
│       └── BlowfishCore.cpp  # 核心加密函数
├── Log/                      # 日志模块
│   └── log.cpp               # 日志输出函数
└── ntos/inc/                 # Windows 内核头文件
    └── 10.0.19041.4291/
        └── ntkrnlmp.h        # ntoskrnl.exe 符号表
```

**核心实现**：

1. **调试对象结构**（Windows 内核原生结构重建）：
   ```cpp
   typedef struct _DEBUG_OBJECT {
       KEVENT EventsPresent;           // 事件信号量
       FAST_MUTEX Mutex;               // 快速互斥锁
       LIST_ENTRY EventList;           // 调试事件链表
       ULONG Flags;                    // 标志位
   } DEBUG_OBJECT, *PDEBUG_OBJECT;
   
   typedef struct _DEBUG_EVENT {
       LIST_ENTRY EventList;           // 事件链表节点
       KEVENT ContinueEvent;           // 继续事件
       CLIENT_ID ClientId;             // 客户端 ID（PID/TID）
       PEPROCESS Process;              // 进程对象指针
       PETHREAD Thread;                // 线程对象指针
       NTSTATUS Status;                // 状态码
       ULONG Flags;                    // 标志位
       PETHREAD BackoutThread;         // 回退线程
       DBGKM_APIMSG ApiMsg;            // 调试消息
   } DEBUG_EVENT, *PDEBUG_EVENT;
   ```

2. **调试事件类型**：
   - `DbgCreateThreadStateChange` (2)：线程创建事件
   - `DbgCreateProcessStateChange` (3)：进程创建事件
   - `DbgExitThreadStateChange` (4)：线程退出事件
   - `DbgExitProcessStateChange` (5)：进程退出事件
   - `DbgExceptionStateChange` (6)：异常事件（断点、单步等）
   - `DbgLoadDllStateChange` (9)：DLL 加载事件
   - `DbgUnloadDllStateChange` (10)：DLL 卸载事件

3. **EPT Hook 内核函数**（绕过反调试检测）：
   - `NtQueryInformationProcess`：拦截调试检测查询（ProcessDebugPort/ProcessDebugObjectHandle）
   - `NtSetInformationThread`：拦截线程隐藏（HideFromDebugger）
   - `DbgkpSendApiMessage`：拦截调试消息发送
   - `PsGetNextProcessThread`：拦截线程遍历

4. **VMCALL 网关接口**（通过 DPC 广播到所有核心）：
   ```cpp
   // 卸载全部 Hook
   bool hvgt::ept_unhook();
   
   // 卸载指定函数 Hook
   bool hvgt::ept_unhook(void* function_address);
   
   // 设置隐形软件断点
   bool hvgt::set_hide_software_breakpoint(MDL_MAP* map_table, void* buffer, size_t size);
   
   // 读取隐形软件断点原字节
   bool hvgt::get_hide_software_breakpoint(void* target_address, void* buffer, size_t size);
   
   // Hook 函数
   bool hvgt::hook_function(void* target, void* proxy, void** origin);
   
   // 刷新 EPT TLB
   void hvgt::invept(bool invept_all);
   
   // 同步 CR3 监控策略
   bool hvgt::synchronize_cr3_exit_policy();
   ```

### 三、UnrealDbg - Delphi 调试器主界面

**功能定位**：调试器的用户交互前端，提供可视化界面管理调试会话、进程/线程/模块、断点、内存查看等功能。

**技术栈**：
- **语言**：Delphi 10.x (Object Pascal)
- **UI 框架**：VCL (Visual Component Library)
- **通信方式**：通过 UnrealDbgDll.dll 与内核驱动交互

**主要功能模块**：
1. **进程管理**：
   - 进程枚举与选择
   - 附加调试（NtDebugActiveProcess）
   - 分离调试
   - 进程信息查看（PID、路径、模块列表）

2. **线程管理**：
   - 线程枚举与状态查看
   - 线程挂起/恢复
   - 线程上下文读取（寄存器、堆栈）
   - 线程创建/退出事件监听

3. **断点管理**：
   - 软件断点（INT3）设置/删除
   - 硬件断点（EPT Watch）设置
   - 内存断点（读/写/执行监控）
   - 断点列表可视化
   - 条件断点支持

4. **调试事件处理**：
   - 等待调试事件（WaitForDebugEvent）
   - 继续执行（DebugContinue）
   - 异常处理（断点、单步、访问违例）
   - DLL 加载/卸载事件

5. **内存查看**：
   - 内存转储（Hex 查看器）
   - 内存搜索
   - 内存修改
   - 内存保护属性查看

6. **反汇编器**：
   - 指令反汇编显示
   - 跳转目标跟踪
   - 符号地址解析

7. **网络验证**：
   - 卡密验证（通过 CardRegistration 模块）
   - Blowfish 加密通信

### 四、UnrealDbgDll - 用户态桥接 DLL

**功能定位**：连接 Delphi 前端与内核驱动的桥接层，封装 IOCTL 通信、符号表解析、网络验证等功能。

**目录结构**：
```
UnrealDbgDll/
├── dllmain.cpp               # DLL 入口
├── Channels/
│   └── DispatchData.cpp      # 数据分发
├── Globals.cpp               # 全局状态
└── NetVerHelp/               # 网络验证辅助
    └── Help_Source/
        └── CJsonObject.cpp   # JSON 解析
```

**核心功能**：
1. **符号表初始化**：
   - 解析 ntoskrnl.exe 的 PDB 符号文件
   - 获取内核函数地址与结构偏移
   - 通过 VMCALL_INIT_OFFSET 传递给驱动

2. **IOCTL 通信封装**：
   - 打开驱动设备句柄（CreateFile）
   - 封装 DeviceIoControl 调用
   - 参数序列化与反序列化

3. **调试 API 封装**：
   ```cpp
   // 附加调试进程
   BOOL DbgAttachProcess(DWORD ProcessId);
   
   // 等待调试事件
   BOOL DbgWaitForDebugEvent(LPDEBUG_EVENT lpDebugEvent, DWORD dwMilliseconds);
   
   // 继续执行
   BOOL DbgContinueDebugEvent(DWORD dwProcessId, DWORD dwThreadId, DWORD dwContinueStatus);
   
   // 设置断点
   BOOL DbgSetBreakpoint(PVOID Address, DWORD Type);
   
   // 删除断点
   BOOL DbgRemoveBreakpoint(PVOID Address);
   ```

### 五、Hook - 用户态钩子 DLL

**功能定位**：用户态进程注入 DLL，配合内核驱动实现用户态函数钩子与调试事件上报。

**目录结构**：
```
Hook/
├── dllmain.cpp               # DLL 入口
├── DllMain/
│   └── Core/
│       ├── DllMainDriverConnection.cpp  # 与驱动通信
│       └── DllMainState.cpp             # DLL 状态管理
├── HookCallSet/
│   └── Hooks/
│       └── HookManager/
│           ├── HiddenInt3MemoryRead.cpp     # 隐藏 INT3 内存读取
│           └── HiddenInt3WriteRemoval.cpp   # 隐藏 INT3 写入移除
├── vmx/
│   ├── VmxBreakpointState.cpp           # 断点状态管理
│   └── Asm/
│       ├── AsmCallset.asm               # VMCALL 汇编
│       └── AsmCallsetX86.asm            # x86 兼容汇编
└── Channels/
    └── DispatchData.cpp      # 数据分发
```

**核心功能**：
1. **隐藏 INT3 断点**：
   - Hook ReadProcessMemory/NtReadVirtualMemory
   - 拦截对断点地址的读取
   - 返回原始字节而非 0xCC

2. **断点透明化**：
   - 与驱动同步断点列表
   - 过滤调试器自身的断点检测
   - 防止目标程序通过 CRC 校验检测断点

3. **用户态 VMCALL**：
   - 封装 VMCALL 指令调用
   - 与内核驱动通信

### 六、Loader - 驱动加载器

**功能定位**：负责加载驱动、初始化符号表、启动调试器主程序。

**目录结构**：
```
Loader/
├── WinMain.cpp               # 主程序入口
├── WinMain/
│   └── WinMainState.cpp      # 主程序状态管理
└── Symbols/                  # 符号解析
    ├── Symbols.cpp           # 符号表初始化
    ├── SymbolState.cpp       # 符号状态管理
    ├── FileUtilities.cpp     # 文件工具
    └── SymbolServerConfig.cpp # 符号服务器配置
```

**启动流程**：
```
Loader.exe
├── DisplayBrand()            # 显示品牌信息
└── StartProcess()            # 启动进程
    ├── 加载 VT_Driver.sys    # 加载虚拟化驱动
    ├── 加载 DbgkSysWin10/Win11.sys  # 加载调试子系统
    ├── 初始化符号表          # 解析 ntoskrnl.exe 符号
    ├── 发送 VMCALL_INIT_OFFSET  # 传递符号偏移到驱动
    └── 启动 UnrealDbg.exe    # 启动调试器主界面
```

### 七、Common - 公共库

**功能定位**：跨模块共享的代码、数据结构、工具函数库。

**主要组件**：
1. **Ring0 内核工具**：
   - `Spinlock`：自旋锁实现
   - `String`：内核字符串处理
   - `Inject/ApcInject`：APC 注入框架
   - `Comms/SerialPort`：串口通信（调试输出）
   - `SymbolicAccess`：PDB 符号解析库

2. **加密库**：
   - `Encrypt/Blowfish`：Blowfish 加密算法
   - `Hash/MD5`：MD5 哈希算法
   - `Hash/Crc32`：CRC32 校验

3. **文件系统**：
   - `FileSystem`：文件读写封装
   - `TinyXML`：XML 解析库

4. **通用工具**：
   - `Core/CommonCpuInfo`：CPU 信息获取
   - `Core/CommonStringConversions`：字符串转换
   - `Detours`：Detours Hook 兼容层

5. **IPC 通信**：
   - `IPC/SharedMemory`：共享内存通信

---

## 技术实现细节

### EPT 页表管理

**EPT 结构**（四级页表）：
```
EPT PML4 (512 entries) → 每项覆盖 512GB
  ↓
EPT PDPT (512 entries) → 每项覆盖 1GB
  ↓
EPT PD (512 entries)   → 每项覆盖 2MB（大页）或指向 PT
  ↓
EPT PT (512 entries)   → 每项覆盖 4KB（小页）
```

**动态拆分机制**：
- 初始状态：EPT PD 条目指向 2MB 大页（Large Page，减少 TLB Miss）
- Hook/Watch 触发拆分：
  1. 分配 EPT PT（512 个 4KB 小页条目）
  2. 将 2MB 大页拆分为 512 个 4KB 小页
  3. 修改 EPT PD 条目指向 EPT PT
  4. 每个小页继承原大页的缓存类型（从 MTRR 读取）
- 引用计数管理：
  - 每个 Split 维护引用计数（`ref_count`）
  - Hook/Watch 安装时 `ref_count++`
  - Hook/Watch 卸载时 `ref_count--`
  - `ref_count == 0` 时尝试合并回 2MB 大页

**内存池预分配**（Pool Manager）：
- 驱动启动时预分配大块内存池，避免 VM-Exit 路径中动态分配
- 分配意图（Intention）分类：
  - `INTENTION_TRACK_HOOKED_PAGES`：Hook 页信息结构
  - `INTENTION_TRACK_HOOKED_FUNCTIONS`：Hook 函数信息结构
  - `INTENTION_EXEC_TRAMPOLINE`：Trampoline 代码缓冲
  - `INTENTION_SPLIT_PML2`：EPT PD 拆分缓冲
- 失败处理：预分配耗尽时返回失败，不触发内存分配

### 隐形断点实现

**原理**：利用 EPT 权限分离读/写/执行访问，实现对目标进程透明的软件断点。

**实现步骤**：
1. **创建伪页**：
   ```cpp
   // 分配伪页内存（4KB，页对齐）
   unsigned char fake_page_contents[PAGE_SIZE];
   
   // 复制原始页内容到伪页
   RtlCopyMemory(fake_page_contents, PAGE_ALIGN(target_address), PAGE_SIZE);
   
   // 在伪页中写入 INT3 (0xCC)
   fake_page_contents[offset] = 0xCC;
   ```

2. **配置 EPT 权限**：
   ```cpp
   // 原页条目（读写访问）
   original_entry.read = 1;
   original_entry.write = 1;
   original_entry.execute = 0;  // 不可执行
   original_entry.page_frame_number = original_pfn;
   
   // 伪页条目（执行访问）
   changed_entry.read = 0;      // 不可读
   changed_entry.write = 0;     // 不可写
   changed_entry.execute = 1;
   changed_entry.page_frame_number = fake_pfn;
   ```

3. **动态切换**：
   - Guest 执行目标地址 → EPT Violation (Execute) → 切换到伪页 → 触发 INT3 → 调试事件
   - Guest 读/写目标地址 → EPT Violation (Read/Write) → 切换到原页 → 返回原始数据
   - 访问完成后设置 MTF，下一条指令执行完恢复 EPT 权限

4. **用户态透明化**：
   - Hook 用户态 `ReadProcessMemory`/`NtReadVirtualMemory`
   - 拦截对断点地址的读取
   - 通过 `VMCALL_GET_SOFTWARE_BREAKPOINT_BUFFER` 读取原字节并返回

### EPT Watch 断点虚拟化

**监控类型**：
- `EPTW_WRITE` (1)：仅监控写入（EPT Write = 0）
- `EPTW_READWRITE` (2)：监控读写（EPT Read = 0, Write = 0）
- `EPTW_EXECUTE` (3)：监控执行（EPT Execute = 0）

**断点槽位管理**：
```cpp
#define EPTWATCHLISTSIZE 512  // 最多支持 512 个监控点

typedef struct _EPT_WATCH_ENTRY {
    PVOID VirtualAddress;      // 监控的虚拟地址
    ULONG64 PhysicalAddress;   // 对应的物理地址
    ULONG Type;                // 监控类型（EPTW_*）
    ULONG bpType;              // 断点类型（0=普通, 3=INT3）
    INT GroupId;               // 分组 ID（同组断点联动）
    BOOLEAN Reserved;          // 预留标志
} EPT_WATCH_ENTRY;

EPT_WATCH_ENTRY eptWatchList[EPTWATCHLISTSIZE];
```

**触发流程**：
```
Guest 访问监控地址 → EPT Violation
→ vmexit_ept_violation_handler()
→ ept::ept_handleWatchEvent()
   ├── 定位 EPT Watch 条目
   ├── 填充断点信息（VirtualAddress、Type、CR3）
   ├── 保存 Guest 上下文（RIP、寄存器）
   └── 返回 true（断点命中）
→ 设置 MTF 恢复权限
→ VMRESUME 返回 Guest
```

### CR3 进程切换跟踪

**功能**：监控 CR3 寄存器加载（进程切换），实现进程级别的调试隔离。

**实现方式**：
1. **启用 CR3 监控**：
   ```cpp
   // 设置 VMCS Primary Processor-Based Controls
   primary_controls.cr3_load_exiting = 1;  // 拦截 MOV to CR3 指令
   ```

2. **CR3 切换策略**：
   - `Cr3TrackerObserve(ULONG64 cr3)`：观察 CR3 值，记录活跃进程
   - `Cr3TrackerSynchronizeExitPolicy()`：同步 CR3 监控策略到所有核心
   - 只在目标进程的 CR3 下触发 EPT Violation，其他进程不受影响

3. **应用场景**：
   - 仅调试特定进程，避免全局性能损耗
   - 进程隔离的断点管理

### VMCS 配置

**控制位调整**（根据硬件能力调整请求值）：
```cpp
// 读取硬件能力 MSR
ULONG64 capability_msr = __readmsr(IA32_VMX_PROCBASED_CTLS);
ULONG32 allowed0 = (ULONG32)capability_msr;       // bit 为 0 表示必须为 0
ULONG32 allowed1 = (ULONG32)(capability_msr >> 32);  // bit 为 1 表示可以为 1

// 调整请求值
ULONG32 adjusted = (requested | allowed0) & allowed1;
```

**关键控制位**：
- **Pin-Based Controls**：
  - `external_interrupt_exiting = 1`：拦截外部中断
  - `nmi_exiting = 1`：拦截 NMI

- **Primary Processor-Based Controls**：
  - `use_msr_bitmaps = 1`：启用 MSR 位图过滤
  - `use_io_bitmaps = 1`：启用 IO 位图过滤
  - `activate_secondary_controls = 1`：启用次级控制

- **Secondary Processor-Based Controls**：
  - `enable_ept = 1`：启用 EPT
  - `enable_vpid = 1`：启用 VPID（减少 TLB 刷新）
  - `unrestricted_guest = 1`：启用非受限客户机（无需实模式模拟）

- **VM-Exit Controls**：
  - `host_address_space_size = 1`：Host 64 位模式
  - `save_debug_controls = 1`：保存调试控制寄存器
  - `ack_interrupt_on_exit = 1`：退出时确认中断

- **VM-Entry Controls**：
  - `ia32e_mode_guest = 1`：Guest 64 位模式
  - `load_debug_controls = 1`：加载调试控制寄存器

---

## 编译构建

### 环境要求

- **操作系统**：Windows 10/11 x64
- **开发工具**：
  - Visual Studio 2022（C++ 项目）
  - Delphi 10.x（Delphi 项目）
- **工具链**：
  - MSVC v143 x64 平台工具集
  - Windows SDK/WDK 10.0.26100.0
- **汇编器**：MASM (ml64.exe)

### 源码编码规则

- C/C++ 源文件、头文件、资源脚本：**UTF-8 BOM**（中文注释/字符串必须带 BOM，纯 ASCII 可无 BOM）
- MASM 汇编文件 (.asm)：**UTF-8 无 BOM**
- Delphi 文件 (.pas/.dfm)：**CP936（GBK）**，保持原有编码
- 根目录 `.editorconfig` 文件固化编码规则，VS2022 自动遵循
- **不需要添加** `/utf-8`、`/source-charset`、`/execution-charset` 编译选项
- 资源脚本中的 `#pragma code_page(65001)` 用于资源文本解析，不影响 C++ 编译

### 构建步骤

**命令行构建**（推荐）：
```bat
# 打开开发人员命令提示符（Developer Command Prompt for VS 2022）
# 导航到项目根目录
cd F:\001\VT\UnrealVTDbg-struc-clean

# 构建所有 C++ 项目（Release x64）
MSBuild.exe UnrealDbg.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

**IDE 构建**：
1. 使用 Visual Studio 2022 打开 `UnrealDbg.sln`
2. 配置管理器选择 `Release | x64`
3. 右键解决方案 → "重新生成解决方案"

**Delphi 项目构建**：
- Delphi 项目不由 VS2022 编译，需单独使用 Delphi IDE 编译
- 项目文件：
  - `UnrealDbg\UnrealDbg.dproj`
  - `CardRegistration\卡密注册.dproj`
  - `D-encryption\D_encryption.dproj`
  - `SymbolTool\SymbolTool.dproj`

**构建输出**：
- `x64\Release\VT_Driver.sys`：VMX 虚拟化引擎驱动
- `x64\Release\DbgkSysWin10.sys`：Windows 10 调试子系统驱动
- `x64\Release\DbgkSysWin11.sys`：Windows 11 调试子系统驱动
- `x64\Release\UnrealDbgDll.dll`：用户态桥接 DLL
- `x64\Release\Hook.dll`：用户态钩子 DLL
- `x64\Release\Loader.exe`：驱动加载器

### 常见构建问题

1. **C1083 错误（找不到 WDK 头文件）**：
   - 原因：未安装指定版本的 WDK
   - 解决：安装 WDK 10.0.26100.0 或修改项目属性中的 SDK 版本

2. **编码相关错误（中文乱码）**：
   - 原因：文件编码不是 UTF-8 BOM
   - 解决：使用 VS2022 "文件 → 高级保存选项" 设置为 "UTF-8 with BOM"

3. **MASM 汇编错误**：
   - 原因：汇编文件编码或语法问题
   - 解决：确保 `.asm` 文件为 UTF-8 无 BOM，检查汇编语法

---
