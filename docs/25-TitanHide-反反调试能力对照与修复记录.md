# TitanHide 反-反调试能力对照与修复记录

范围：以 `F:\001\VT\中间目录\titanhide` 的**内核驱动 + OllyDbg PEB 处理**为对照，
逐项检查 `DbgkSysWin10` / `DbgkSysWin11` 当前源码，并只修改能够由源码与规范确定的缺口。
Win10 与 Win11 分开维护：两侧分别核对自身的头文件、结构偏移、函数指针签名与调用链，
不做整文件照搬。

## 一、TitanHide 真实处理了哪些反调试检测

以下均为 TitanHide 源码事实（文件:函数）：

| 能力 | TitanHide 实现位置 | 处理方式 |
|---|---|---|
| ProcessDebugFlags | `TitanHide/hooks.cpp:HookNtQueryInformationProcess` | 查询返回 `TRUE`；`HookNtSetInformationProcess` 对 `ProcessDebugFlags` 直接成功 |
| ProcessDebugPort | 同上 | 查询成功后把输出置 0 |
| ProcessDebugObjectHandle | 同上 | 直接返回 `STATUS_PORT_NOT_SET`，输出句柄置 0 并写 `ReturnLength=sizeof(HANDLE)` |
| DebugObject 类型 | `hooks.cpp:HookNtQueryObject` | `ObjectTypeInformation` / `ObjectTypesInformation` 中扣除被隐藏进程自己的 DebugObject 对象/句柄贡献 |
| SystemKernelDebuggerInformation / Ex | `hooks.cpp:HookNtQuerySystemInformation` | `DebuggerEnabled=false, DebuggerNotPresent=true`；Ex 版三个布尔全部 false |
| SystemDebugControl | `hooks.cpp:HookNtSystemDebugControl` | 除 Triage/LiveKernelDump 外返回 `STATUS_DEBUGGER_INACTIVE` |
| NtClose 保护句柄 | `hooks.cpp:HookNtClose` / `HookNtDuplicateObject` | `OBJ_PROTECT_CLOSE` 句柄返回 `STATUS_HANDLE_NOT_CLOSABLE` 而不是抛异常；`DUPLICATE_CLOSE_SOURCE` 时剥掉该标志 |
| ThreadHideFromDebugger | `hooks.cpp:HookNtSetInformationThread`、`HookNtQueryInformationThread`、`HookNtCreateThreadEx`、`threadhidefromdbg.cpp:UndoHideFromDebuggerInRunningThreads` | Set 直接成功但不落 ETHREAD 标志；Query 返回 TRUE；CreateThreadEx 剥 `THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER`；DKOM 清运行中线程的 `CrossThreadFlags.HideFromDebugger` |
| NtGet/NtSetContextThread（DRx） | `hooks.cpp:HookNtGetContextThread` / `HookNtSetContextThread` | 用户态调用者与被查询线程都属于隐藏 PID 时，临时剥离 `CONTEXT_DEBUG_REGISTERS`，读路径清零 DR0-DR3/DR6/DR7（含 LastBranch 字段），写路径阻止目标写 DRx |
| WOW64 DRx | `hooks.cpp` 的 `ThreadWow64Context` 分支 | 同上，对 32 位 CONTEXT 布局处理 |
| PEB/堆调试痕迹（Olly/TitanEngine 变体） | `TitanHide_OllyDbg/pebhider.cpp:HidePEB` | 清 `PEB.BeingDebugged`、`PEB.NtGlobalFlag & ~0x70`、堆 `Flags &= HEAP_GROWABLE`、`ForceFlags=0`（64 位与 WOW64 PEB 各一份） |

判定规则（TitanHide 源码事实）：

- 所有 hook 在 `ExGetPreviousMode() == KernelMode` 时原样调用原函数，不拦截内核内部调用。
- 隐藏是**按 PID 的显式列表**（`Hider::IsHidden(pid, type)`）；x64dbg 插件在系统断点后 hide，
  停止调试时 unhide。

## 二、本项目原来的真实状态（修复前）

`DbgkSysWin10` / `DbgkSysWin11` 原本已经有两类基础能力：

1. attach 路径在 `DbgkpSetProcessDebugObject` 中把 `EPROCESS->DebugPort` 置空，
   所以 `ProcessDebugPort / ProcessDebugObjectHandle / ProcessDebugFlags` 在 attach 路径
   有一定自然隐藏效果；
2. `DbgkpMarkProcessPeb` 会清 `PEB.BeingDebugged`。

但源码确定的缺口是：

- `Hooks/EptHook/Core/EptHookLifecycle.cpp:SetupEptHook` **没有安装**
  `Hook_DbgkOpenProcessDebugPort / Hook_NtQueryInformationThread / Hook_NtGetContextThread /
  Hook_NtSetContextThread / Hook_NtCreateThreadEx`；这些 wrapper 已有定义但从未执行。
- `Protect/Thread/ProtectDrxX64.cpp` 原实现只改 `EFlags=0x229` / 写 shadow context，
  不剥离 DRx；TitanHide 的 DRx 语义没有落地。且它只在被安装时才有意义，而当时根本未安装。
- 没有 `NtQueryInformationProcess / NtSetInformationProcess / NtQueryObject /
  NtQuerySystemInformation / NtSystemDebugControl / NtClose / NtDuplicateObject /
  NtSetInformationThread` 的 EPT Hook。
- `Hooks/EptHook/Core/EptHookNtosDebug.cpp` 中 `Hook_DbgkOpenProcessDebugPort` 使用的是
  “对任何进程都返回 `STATUS_PORT_NOT_SET`”的 stub；即使安装也会破坏非目标进程的原生调试语义。
- create 路径（x64dbg `File -> Open`、CE 创建调试）不会进入 `DbgkpSetProcessDebugObject`，
  目标进程不在 `g_DebugProcessList`，`EPROCESS->DebugPort` 仍指向 `Hvm_DebugObject`；
  因此上述 attach 路径的自然隐藏不覆盖 create 路径，PEB/NtGlobalFlag/Heap Flags 也没有被清理。
- `NewNtQueryInformationThread`（旧实现）对 `ThreadSuspendCount` 无条件返回失败，
  但该 hook 未安装；若直接安装会破坏线程挂起计数查询语义。

## 三、实际修改（按 TitanHide 能力逐项）

### FIX-AD-01 NtQueryInformationProcess 三个查询类

- 新增文件：`DbgkSysWin10/Hooks/EptHook/Core/EptHookAntiDebug.cpp`
  `DbgkSysWin11/Hooks/EptHook/Core/EptHookAntiDebug.cpp`
- 处理：`HvmNtQueryInformationProcess`
  - `ProcessDebugObjectHandle`：目标进程返回 `STATUS_PORT_NOT_SET`、输出 NULL；
    按 `ProcessInformationLength` 同时支持原生 8 字节 HANDLE 与 WOW64 4 字节 HANDLE，
    避免 x64 驱动的 `sizeof(HANDLE)` 条件漏掉 32 位目标；
  - `ProcessDebugFlags`：成功后强制 `1`；
  - `ProcessDebugPort`：成功后输出 0，同样支持原生 8 字节与 WOW64 4 字节输出；
  - 隐藏判定基于 `ProcessHandle` 指向的进程（与 TitanHide 一致），不是调用者 PID；
  - `NtCurrentProcess()` / `NtCurrentThread()` 的 -1/-2 与 WOW64 零扩展形式
    (`0xFFFFFFFF` / `0xFFFFFFFE`) 都按当前对象解析，避免伪句柄查询绕过硬编码检查。
- `NtSetInformationProcess(ProcessDebugFlags)` 直接返回成功：`HvmNtSetInformationProcess`。
- 内核态调用（`PreviousMode == KernelMode`）全部原样转发。

### FIX-AD-02 DbgkOpenProcessDebugPort 正确语义

- Win11：`DbgkSysWin11/DbgkApi/Api/OpenProcessDebugPort.cpp` 改为
  “虚拟调试目标返回 `STATUS_PORT_NOT_SET`，其余调用 `Original_DbgkOpenProcessDebugPort`”。
- Win10：新增 `DbgkSysWin10/DbgkApi/Api/OpenProcessDebugPort.cpp`，并按 Win10 自己的
  `Globals.h::PFN_DBGKOPENPROCESSDEBUGPORT(PMY_EPROCESS, ...)` 与
  `InitNtoskrnl.cpp` 符号解析落地；`Core/Driver.h` 增加声明。
- 两侧 `Hook_DbgkOpenProcessDebugPort` 都传入 `&Original_DbgkOpenProcessDebugPort`，
  不再使用“全部进程一律 PORT_NOT_SET”的 stub。

### FIX-AD-03 NtQueryObject / DebugObject

- `HvmNtQueryObject` 处理：
  - `ObjectTypeInformation`：命中 `Hvm_DebugObject` 时把类型名原地改短为 `DebugObject`，
    并把该类型的对象/句柄计数清零；
  - `ObjectTypesInformation`：按内核返回布局遍历 `OBJECT_TYPE_INFORMATION`，
    直接删除 `Hvm_DebugObject` 条目、修正后续条目的 `TypeName.Buffer` 指针并递减
    `NumberOfTypes`，避免留下重复的 `DebugObject` 名称或额外类型计数；
  - `SystemHandleInformation` / `SystemExtendedHandleInformation`：把本驱动对象类型的
    类型索引改写为原生 `DbgkDebugObjectType` 索引，避免句柄枚举通过未知类型索引发现
    `Hvm_DebugObject`；
  - 处理 `ReturnLength` 与 `TypeName.Buffer` 重叠的 VMProtect 3.9.5+ 行为
    （先备份、最后恢复 `ReturnLength`）；
  - 仅对“当前进程是虚拟调试目标”的用户态调用生效。

### FIX-AD-04 NtQuerySystemInformation / NtSystemDebugControl

- `SystemKernelDebuggerInformation`：`DebuggerEnabled=false, DebuggerNotPresent=true`；
- `SystemKernelDebuggerInformationEx`：三个布尔全部 false；
- `SystemProcessorFeaturesInformation`：清除 `PF_VIRT_FIRMWARE_ENABLED`(bit 21)；
- `SystemHypervisorInformation`：`HypervisorConnected/HypervisorDebuggingEnabled/HypervisorPresent`
  全部置 false，`EnabledEnlightenments=0`；
- `VT_Driver` 在隐藏模式下清除 `KUSER_SHARED_DATA.ProcessorFeatures[21]`，
  防止 `IsProcessorFeaturePresent(PF_VIRT_FIRMWARE_ENABLED)` 绕过 CPUID 伪装；
- `RDMSR IA32_VMX_BASIC(0x480)` 至 `IA32_VMX_PROCBASED_CTLS3(0x492)` 在隐藏模式下
  按“处理器不支持 VMX”注入 #GP，不再透传真实 VMX capability MSR；
- `NtSystemDebugControl`：除 `SysDbgGetTriageDump(29)` / `SysDbgGetLiveKernelDump(37)` 外，
  虚拟调试目标调用返回 `STATUS_DEBUGGER_INACTIVE`。

### FIX-AD-05 NtClose / NtDuplicateObject

- `HvmNtClose`：
  - 先取 `g_AntiDebugNtCloseMutex`，再 `ObQueryObjectAuditingByHandle`；
  - `STATUS_INVALID_HANDLE` 直接返回状态，不进入 `ObCloseHandle` 的异常路径；
  - `OBJ_PROTECT_CLOSE` 句柄返回 `STATUS_HANDLE_NOT_CLOSABLE`，不真正关闭；
  - 其余句柄走 `ObCloseHandle(Handle, PreviousMode)`。
- `HvmNtDuplicateObject`：`DUPLICATE_CLOSE_SOURCE` 且源句柄带 `OBJ_PROTECT_CLOSE` 时
  清除该选项，避免内核内部关闭源句柄时抛异常。

### FIX-AD-06 ThreadHideFromDebugger

- `Protect/Thread/ProtectDrxX64.cpp`（两侧各自维护）：
  - `HvmNtSetInformationThread`：目标进程 `ThreadHideFromDebugger` 请求直接成功，
    不设置 ETHREAD 标志；
  - `HvmNtSetInformationThread(ThreadWow64Context)`：WOW64 的 SetThreadContext 路径
    临时剥离 `CONTEXT_DEBUG_REGISTERS`，防止 32 位目标写入/恢复 DRx；
  - `NtQueryInformationThread(ThreadHideFromDebugger)`：查询成功后强制返回 TRUE，
    并保留 ReturnLength；
  - `NtCreateThreadEx` handler（`Core/DebugEvents/ThreadCreationEvents.cpp`）：
    目标进程带 `THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER` 时剥离该标志。
- `PrepareVirtualDebugTargetProcess`：attach 与 create 两条路径首次进入时，
  用 `ethread_offset::CrossThreadFlags` 清运行中线程的 `HideFromDebugger` 位（DKOM）。

### FIX-AD-07 DRx（NtGetContextThread / NtSetContextThread / WOW64）

- `NtGetContextThread`：调用者与被查询线程都属于虚拟调试目标时，
  临时剥离 `CONTEXT_DEBUG_REGISTERS`，成功后清零 DR0-DR3/DR6/DR7 与 LastBranch 字段。
- `NtSetContextThread`：调用者是虚拟调试目标时临时剥离 DR 请求，阻止目标写 DRx。
- `NtQueryInformationThread(ThreadWow64Context)`：同样按 TitanHide 的 32 位 CONTEXT 偏移
  清零 DR0-DR3/DR6/DR7。
- 调试器自身不是虚拟调试目标，因此 x64dbg/CE 读取上下文的路径不受影响；
  其断点视图仍由 `Hook/DebugBreak/Core/ThreadContextHooks.cpp` 合成。

### FIX-AD-08 create 路径的 PEB 调试痕迹

- `PrepareVirtualDebugTargetProcess`（`Hooks/EptHook/Core/EptHookAntiDebug.cpp`）：
  - 64 位 PEB：`BeingDebugged=0`、`NtGlobalFlag &= ~0x70`、
    堆 `Flags &= HEAP_GROWABLE`、`ForceFlags=0`；
  - WOW64：通过 `EPROCESS->WoW64Process->_EWOW64PROCESS.Peb` 获取 32 位 PEB，
    使用 32 位 PEB/堆偏移处理；
  - 内存读写通过 `Sys_MmCopyVirtualMemory(..., KernelMode, ...)`，在 PASSIVE_LEVEL 调用，
    Win10/Win11 结构偏移各自校验。
- 调用点：
  - attach：`DbgkpSetProcessDebugObject` 成功后（两侧各自的 `DebugActiveProcessAttach.cpp`）；
  - create：`DbgkCreateThread` 首个线程、`OldFlags.CreateReported == 0` 时
    （两侧各自的 `DebugThreadCreate.cpp`）。

### FIX-AD-09 统一虚拟调试目标判定

- `HvmIsVirtualDebugTargetProcess`：
  - attach 路径：`EPROCESS->DebugPort == NULL` 且命中 `g_DebugProcessList`；
  - create 路径：`EPROCESS->DebugPort` 指向本驱动 `Hvm_DebugObject` 类型
    （在 `DbgkpProcessDebugPortMutex` 保护下读取，并用
    `ObReferenceObjectByPointer` 校验类型）；
  - 无活跃 `Hvm_DebugObject` 时用 `g_LiveVirtualDebugObjectCount` 快速放行，
    避免给无调试会话的系统热路径增加锁开销。
- `g_LiveVirtualDebugObjectCount/HandleCount` 在 `NtCreateDebugObject` 成功返回句柄时 +1，
  在 `DbgkpCloseObject` 每次关闭时减句柄、最后句柄关闭时减对象。

### FIX-AD-10 旧实现清理

- 删除 `NewNtQueryInformationThread`（无条件拒绝 `ThreadSuspendCount` 的旧实现）及其声明；
  `Hook_NtQueryInformationThread` 改为挂接 `ProtectDrxX64.cpp` 的 TitanHide 对照实现。
- `Hook_NtGetContextThread / Hook_NtSetContextThread / Hook_NtCreateThreadEx /
  Hook_NtQueryInformationThread / Hook_DbgkOpenProcessDebugPort` 全部真正安装。

## 四、Ownership / Lifetime / Concurrency / IRQL 复核

- `g_LiveVirtualDebugObjectCount/HandleCount`：
  - 创建 +1 只在 `ObInsertObject` 成功且用户态句柄写回成功后执行；
  - 关闭 -1 在 `DbgkpCloseObject` 内做饱和递减；对象计数只在 `SystemHandleCount <= 1` 时减；
  - 全部使用 `Interlocked*`，无 lost update。
- `HvmNtClose`：
  - KMUTEX 在 hook 安装时 `KeInitializeMutex`；
  - 只对 `PreviousMode != KernelMode` 且 `IRQL <= APC_LEVEL` 的调用生效，
    满足 `KeWaitForSingleObject` 的 IRQL 约束；
  - audit 检查与 close 在同一临界区，消除“检查后句柄被其他线程关闭”的 TOCTOU。
- `HvmIsVirtualDebugTargetProcess`：
  - 读 `EPROCESS->DebugPort` 时持有 `DbgkpProcessDebugPortMutex`；
  - `ObReferenceObjectByPointer` 成功后成对 `ObDereferenceObject`；
  - `KeGetCurrentIrql() > APC_LEVEL` 时直接返回 FALSE，避免在 DISPATCH_LEVEL 取 FAST_MUTEX。
- `PrepareVirtualDebugTargetProcess`：
  - 只在 PASSIVE/APC 级别调用；
  - `PsGetNextProcessThread` 的所有权契约为“消费传入的旧线程引用，返回新的已引用线程”，
    因此循环把 current 直接交给下一次调用，不再手动 `ObDereferenceObject`；
  - `Sys_MmCopyVirtualMemory` 为空时不执行；
  - PEB 写入失败不会改变调试会话状态（幂等、无回滚副作用）。

## 五、编译验证（SOURCE-LEVEL）

环境：`D:\Microsoft Visual Studio2022\Community\MSBuild\Current\Bin\MSBuild.exe`，
WDK `10.0.26100.0`，Release x64。

```bat
MSBuild.exe DbgkSysWin10\DbgkSysWin10.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64
MSBuild.exe DbgkSysWin11\DbgkSysWin11.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64
```

结果：

- Win10：构建成功，产出 `DbgkSysWin10\x64\Release\DbgkSysWin10.sys`；
- Win11：构建成功，产出 `DbgkSysWin11\x64\Release\DbgkSysWin11.sys`；
- 新增/修改的源文件没有 C4273 / C4819 警告（UTF-8 BOM 与 CRLF 已按项目规则处理）。

2026-10-16 复验又完成：

- `UnrealDbg.sln` Release x64 全量构建成功；
- `DbgkSysWin10/DbgkSysWin11/VT_Driver/Hook64/UnrealDbgDll/Loader` 全部通过；
- `tests/AntiDebugSimulation` 编译并运行，73 个模拟数据检查全部通过；
- 该模拟器与内核共用 `Common/Ring0/Hvm/DebuggerVisibilityPolicy.h`，覆盖
  伪句柄、ProcessDebug*、系统调试器/处理器特性/超visor 信息、WOW64 DRx、
  ThreadHideFromDebugger、NtCreateThreadEx、VMCALL RIP 候选、GUEST_CR3 选择、
  VMX capability MSR 范围。

以上只证明 **SOURCE-LEVEL PASS**；没有在本机进入 VT、没有加载驱动、没有运行目标进程，
因此不声明 Runtime PASS 或 Hardware PASS。

## 六、真正需要运行时/硬件确认的剩余项

1. Win10 运行时的 `ntkrnlmp.pdb` 是否包含 `DbgkOpenProcessDebugPort` 符号。
   若缺失：`Hook_DbgkOpenProcessDebugPort` 会记录空指针并跳过，但
   `NtQueryInformationProcess(ProcessDebugPort/ProcessDebugObjectHandle)` 仍覆盖查询路径；
   该项属于符号版本依赖，不是源码逻辑缺口。
2. P-Core/E-Core 迁移下，目标进程的 PEB/线程标志清理与 DRx 隔离的实际时序
   （本次实现按进程/线程对象操作，不依赖当前 CPU；仍需上机确认无平台差异）。
3. 与真实反调试样本（VMProtect、NCC DebugObject 检测、ThreadHideFromDebugger）
   的逐项运行结果。

## 七、已知的源码层限制（不冒充已解决）

- 本项目仍使用独立对象类型 `Hvm_DebugObject`，但查询路径已消除最常见的可见差异：
  `ObjectTypesInformation` 删除该类型条目，`System[Extended]HandleInformation`
  把该类型索引改写为原生 `DebugObject` 类型索引，`ObjectTypeInformation`
  返回 `DebugObject` 名称并扣除本驱动计数。未做的是把虚拟调试对象本体迁移到原生
  `DbgkDebugObjectType`；这会改变 `DbgkpCloseObject` 接管方式，属于独立的架构变更。
- unhide/停止调试时不恢复 PEB 调试痕迹（TitanHide 的 Olly 实现同样只在 hide 时清理；
  其 unhide 仅把 `BeingDebugged` 设回 TRUE，不恢复堆 Flags）。
- TitanHide 的隐藏列表是用户显式选择 PID + 功能位；本项目按“虚拟调试目标”自动启用全部
  目标隐藏能力，没有新增 GUI 开关（符合“Delphi GUI 暂不调整”的项目约束）。

## 八、2026-10-16 蓝屏根因修复与反调试 Hook 恢复

### FIX-AD-11 VMCALL RIP 查找使用错误的 CR3

- 源码事实：`VT_Driver/Ept/Hook/Instruction/EptHookInstruction.cpp:handler_vmcall_rip`
  原实现使用 `hv::ghv.system_cr3.flags` 做 guest RIP 的 GVA→GPA 翻译。
- 源码事实：`VT_Driver/Vmm/VmmHostPageTables.cpp` 将该变量明确设置为
  “attach 到 dwm.exe 后读取的硬件 CR3”，它是宿主侧 VMM 地址空间来源，不是执行当前 guest
  指令的 CR3。在 KVA shadow、进程地址空间切换或 dwm 生命周期变化后，该翻译可能返回 0
  或错误 PFN。
- 影响链：翻译失败 → 找不到 EPT 页/函数 hook 记录 → `vmexit_vmcall_handler` 误判为
  非项目 VMCALL → 向 guest 注入 #UD → 0x3B / 0xC000001D。
- 修复：`handler_vmcall_rip` 改为优先使用 VMCS `GUEST_CR3`；增加
  `GUEST_RIP` 与 `GUEST_RIP - VM_EXIT_INSTRUCTION_LENGTH` 两个精确候选；
  guest CR3 翻译或 PFN 匹配失败时按虚拟地址扫描本 CPU hook 记录；仍失败时通过伪页中的
  `0F 01 C1` 定位记录或恢复原页，禁止把自定义 VMCALL 变成 guest #UD。
- 连锁修复：恢复安装 `Hook_NtQueryInformationProcess()`、
  `Hook_NtQuerySystemInformation()`、`Hook_NtClose()`、`Hook_NtDuplicateObject()`。
  之前把这四个 Hook 从安装列表移除只是规避 VMCALL 查找失败的症状，不是根因修复。

### FIX-AD-12 WOW64 32 位进程查询/上下文缺口

- `ProcessDebugObjectHandle` 与 `ProcessDebugPort` 现在根据实际长度选择 4/8 字节输出，
  不再以 x64 `sizeof(HANDLE)` 作为唯一条件。
- `NtSetInformationThread(ThreadWow64Context)` 补充 DRx 临时剥离路径；
  `NtCurrentThread()`/WOW64 零扩展伪句柄补充当前线程解析。
- 上述两点针对 32 位目标（例如 dump 中的 `arena_breakout`）是必要路径，不是可选优化。

### FIX-AD-13 101726-9562-01.dmp：线程对象二次解引用

- Dump 事实：`REFERENCE_BY_POINTER (0x18)`，Arg4=`0xFFFFFFFFFFFFFFFF`；
  栈为 `ObCloseHandleTableEntry → ObfDereferenceObjectWithTag`，进程 `111.exe`，
  返回点位于 `NewPspExitThread` 调用 `Original_PspExitThread` 后。
- 对象事实：P2=`ffffaa8c98dca080`，`OBJECT_HEADER.PointerCount=-1`，HandleCount=0，
  `!thread` 明确识别为 TERMINATED 的 KTHREAD。
- 规范/实现事实：`nt!PsGetNextProcessThread` 反汇编对返回的新线程执行
  `ObReferenceObjectSafeWithTag`，并对传入的非 NULL 旧线程执行
  `ObfDereferenceObjectWithTag`；即该 API 消费旧引用、返回新引用。
- 根因：`PrepareVirtualDebugTargetProcess` 旧循环在调用
  `PsGetNextProcessThread(Process, previous)` 后，又手动 `ObDereferenceObject(previous)`；
  每个线程被多减一次，x64dbg 退出扫句柄时触发 over-dereference。
- 修复：循环改为 `current = PsGetNextProcessThread(Process, current)`，
  不再手动 Dereference。Win10/Win11 分别修复并编译。
- 模拟：`AntiDebugSimulation` 增加所有权模型，当前 66 checks / 0 failures。

### FIX-AD-14 对象类型/句柄枚举可见性

- `NtQueryObject(ObjectTypesInformation)` 不再把 `Hvm_DebugObject` 改名为重复的
  `DebugObject`，而是直接删除该类型条目、修正后续条目 `TypeName.Buffer` 并递减
  `NumberOfTypes`，消除额外类型计数和重复名称。
- `NtQuerySystemInformation(SystemHandleInformation)` 与
  `SystemExtendedHandleInformation` 将 `Hvm_DebugObject` 的类型索引改写为原生
  `DbgkDebugObjectType` 索引，避免句柄枚举通过未知类型索引发现驱动对象。
- 下一轮运行会在驱动加载后写入一条
  `[AntiDebug] debug object type indexes: virtual=... standard=...` 记录，并在目标
  `NtQueryInformationProcess(ProcessDebug*)` 首次命中时写入 `[AntiDebug]` 记录，
  用于确认最新驱动和目标识别路径确实生效。
- 当前模拟验证：73 checks / 0 failures。

### FIX-AD-15 TitanHide 目标识别时序与 WOW64 调试对象句柄

- 移除 `IsVirtualDebugTargetProcess` 对 `g_LiveVirtualDebugObjectCount` 的前置放行门槛。
  TitanHide 是按 PID 显式隐藏；本项目不能因为计数尚未就绪或瞬时为零就错误放行，
  目标识别现在始终检查 `EPROCESS->DebugPort` 对象和 `g_DebugProcessList`。
- `DebugThreadCreate` 对虚拟调试目标不再只在首个线程创建时清理，而是每次线程创建都
  调用 `PrepareVirtualDebugTargetProcess`，防止后续线程重新带入
  `ThreadHideFromDebugger` / PEB 调试痕迹。
- `NtCreateDebugObject` 区分 WOW64 调用者：32 位调试器只探测/写入 4 字节 HANDLE，
  64 位调用者写入 8 字节 HANDLE，修复 x32dbg 类调用者返回句柄宽度不一致的问题。
- 需要明确：`UnrealDbg.vmp.exe` / `UnrealDbgDll.dll` 的提示框来自 VMProtect，不是 x64dbg。
  TitanHide 本身也不会“自动绕过 VMProtect 的所有内部检查”；它只保证被隐藏 PID 的
  syscall 可见调试状态、PEB、DRx 等项符合其 hook 覆盖范围。

## 九、2026-10-17 全量逐项复核：TitanHide 真实处理了什么

本节结论全部来自逐行阅读 `F:\001\VT\中间目录\titanhide` 源码（SOURCE FACT），
不是从经验或其它开源项目推断。

### 9.1 TitanHide 的真实机制

- 驱动侧用 InfinityHook 改写 **syscall 表**里的目标服务地址，不进入 VT，不做 EPT Hook。
- 隐藏范围是**显式 PID 表**：`hider.cpp` 的 `HideEntries[]` + `Hider::IsHidden(pid, type)`，
  每个条目记录 `(Pid, Type 位掩码)`。
- x64dbg 插件（`TitanHide_x64dbg/plugin.cpp`）只做两件事：
  `CBCREATEPROCESS`/`CBATTACH` 记录 **被调试进程 PID**；
  `CBSYSTEMBREAKPOINT` → `HidePid`，`CBSTOPDEBUG` → `UnhidePid`。
  也就是说：**TitanHide 隐藏的是被调试目标进程，不是调试器进程**。
- 每个 hook 的判定都用 `ExGetPreviousMode() != KernelMode` 把内核内部调用放行。

### 9.2 注册的 syscall hook 与本项目逐项对应

`TitanHide/hooks.cpp` 的 `registrations[]` 共 13 项（含 `ZwSetInformationThread`），
逐项对应关系与源码位置如下：

| TitanHide hook | TitanHide 行为 | 本项目实现 | 位置 |
| --- | --- | --- | --- |
| `NtQueryInformationProcess` | `ProcessDebugFlags`→1；`ProcessDebugPort`→0；`ProcessDebugObjectHandle`→提前返回 `STATUS_PORT_NOT_SET` 并写 NULL 句柄 | `HvmNtQueryInformationProcess` | `DbgkSysWin10/Hooks/EptHook/Core/EptHookAntiDebug.cpp:572` |
| `NtQueryInformationThread` | `ThreadHideFromDebugger`→TRUE；`ThreadWow64Context` 清零 DR0-DR3/DR6/DR7（要求 caller 与 target 都隐藏） | `NtQueryInformationThread` | `DbgkSysWin10/Protect/Thread/ProtectDrxX64.cpp:85` |
| `NtQueryObject` | `ObjectTypeInformation` / `ObjectTypesInformation` 扣掉 DebugObject 计数（NCC 建议） | `HvmNtQueryObject`（并额外改写类型索引） | `EptHookAntiDebug.cpp:769` |
| `NtQuerySystemInformation` | 仅 `SystemKernelDebuggerInformation` / `...Ex` | `HvmNtQuerySystemInformation`（并覆盖 hypervisor/processor feature/句柄类型索引） | `EptHookAntiDebug.cpp:962` |
| `NtSetInformationThread` / `ZwSetInformationThread` | `ThreadHideFromDebugger` 直接吞掉；`ThreadWow64Context` 剥 `CONTEXT_DEBUG_REGISTERS` | `HvmNtSetInformationThread` | `ProtectDrxX64.cpp:323` |
| `NtSetInformationProcess` | `ProcessDebugFlags`→`STATUS_SUCCESS` | `HvmNtSetInformationProcess` | `EptHookAntiDebug.cpp:733` |
| `NtClose` | `OBJ_PROTECT_CLOSE` 句柄→`STATUS_HANDLE_NOT_CLOSABLE`，不让内核走异常路径 | `HvmNtClose` | `EptHookAntiDebug.cpp:1195` |
| `NtDuplicateObject` | 去掉 `DUPLICATE_CLOSE_SOURCE` | `HvmNtDuplicateObject` | `EptHookAntiDebug.cpp:1255` |
| `NtGetContextThread` | DRx（含 LastBranch*）清零 | `NtGetContextThread` | `ProtectDrxX64.cpp:212` |
| `NtSetContextThread` | 剥 `CONTEXT_DEBUG_REGISTERS` | `NtSetContextThread` | `ProtectDrxX64.cpp:272` |
| `NtSystemDebugControl` | 除转储命令外返回 `STATUS_DEBUGGER_INACTIVE` | `HvmNtSystemDebugControl` | `EptHookAntiDebug.cpp:1156` |
| `NtCreateThreadEx` | 剥离 `THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER` | `ThreadCreationEvents` | `DbgkSysWin10/Core/DebugEvents/ThreadCreationEvents.cpp:34` |

非 hook 部分（同样来自 TitanHide 源码）：

| TitanHide 组件 | 行为 | 本项目对应实现 |
| --- | --- | --- |
| `threadhidefromdbg.cpp` | 用 DKOM 清除运行中线程 `ETHREAD.CrossThreadFlags` 的 HideFromDebugger | `PrepareVirtualDebugTargetProcess`（`EptHookAntiDebug.cpp:1319`） |
| `pebhider.cpp`（OllyDbg 变体） | `PEB.BeingDebugged`、`PEB.NtGlobalFlag`、`Heap.Flags`、`Heap.ForceFlags` | `HidePebDebugArtifacts`（native + WOW64 PEB，`:490`） |
| `hider.cpp` 的 `HidePid`/`UnhidePid` | 显式 PID + 类型位掩码登记表 | 虚拟调试目标判定（`EPROCESS->DebugPort` 对象 + `g_DebugProcessList`）＋调试器 PID 表 |

### 9.3 本项目超出 TitanHide 的部分（不是 TitanHide 的做法）

以下能力 TitanHide **没有**，属于本项目自行扩展，不能把“TitanHide 能过”当成它们正确的证据：

- win32k 窗口面：`ValidateHwnd` / `NtUserFindWindowEx` / `NtUserWindowFromPoint` /
  `NtUserSetWindowsHookEx` / `NtUserBuildHwndList` / `NtUserGetClassName`（对付窗口类名扫描）。
- 虚拟化痕迹：`SystemHypervisorInformation`、`SystemProcessorFeaturesInformation`、
  `KUSER_SHARED_DATA.ProcessorFeatures[21]`、`RDMSR 0x480..0x492` 注入 #GP、
  CPUID VMX/hypervisor-present 位。
- `SystemHandleInformation` / `SystemExtendedHandleInformation` 的 `Hvm_DebugObject`
  类型索引改写。

### 9.4 TitanHide 明确不覆盖的检测面（因此本项目也不该声称已解决）

- 调试器**进程名/模块名**扫描（TitanHide 不隐藏进程名，本项目也不隐藏）。
- 目标自身内存扫描（自身代码里找 `0xCC`、比较校验和、CRC）。
- 时间检测（RDTSC、QueryPerformanceCounter、计时异常）。
- VMProtect 自身 VM 内部的完整性校验与虚拟化代码自检。
- 目标主动查询“有哪些调试器窗口”之外的 GUI/输入法/消息钩子异常。

## 十、FIX-AD-16 调试器 PID 表：去重与陈旧登记清理

FIX-ID：AD-16

- File：
  `DbgkSysWin10/DbgkApi/Api/DebugProcessRegistry.cpp`、
  `DbgkSysWin11/DbgkApi/Api/DebugProcessRegistry.cpp`、
  `DbgkSysWin10/Core/IoctlHandlers/DebuggerInfoHandlers.cpp`、
  `DbgkSysWin11/Core/IoctlHandlers/DebuggerInfoHandlers.cpp`、
  `DbgkSysWin*/Globals/Globals.h`（`DEBUGGER_TABLE_ENTRY`）、
  `DbgkSysWin10/DbgkApi/Api/DebugObjectCommon.h`、`DbgkSysWin11/DbgkApi/DbgkApi.h`
- Function：`InsertDebuggerList`、`PurgeStaleDebuggerList`（新增）、`InitDebuggerInfo`
- Severity：S2（调试语义/隐藏链路正确性）
- Category：Logic Defect（Ownership/Lifetime）

**当前逻辑（Before）**：`g_DebuggerList` 只增不减。`InsertDebuggerList` 无条件
`InsertNode`；全项目没有 `PsSetCreateProcessNotifyRoutine`。登记来源有两条：
GUI 进程加载 `UnrealDbgDll` 后由 `Initialize_F3` 调
`SendDebuggerDataToDriver(GetCurrentProcessId())`（`UnrealDbgDll/dllmain.cpp:586`）；
GUI 通过 `StartProcess` 启动第三方工具、向工具注入 Hook 后，再用
`SendDebuggerDataToDriver(processInformation.dwProcessId)`
（`UnrealDbgDll/StartProcess.cpp:323`）登记工具 PID。

**根因**：调试器 PID 表没有与“调试器进程实例”的生命周期绑定，也没有唯一性不变量。

**影响链**：

```text
debugger 反复启动/注入
 ↓
g_DebuggerList 累积重复表项（调试器退出后 PID 仍留在表内）
 ↓
Windows 复用 PID
 ↓
IsDebugger(复用该 PID 的无关进程) == TRUE
 ↓
NewValidateHwnd: 该进程被当作“调试器”，跳过窗口隐藏判断
 ↓
被调试目标的窗口扫描可见调试器窗口 / preProcessCallback 剥 VM_READ|VM_WRITE
```

**修复（After）**：

- `InsertDebuggerList` 增加唯一性不变量：同 PID 已存在时原地刷新
  `fileData`/`fileData2`/`RegisteredProcess` 并返回 FALSE（调用方按既有约定
  `free_pool` 自己分配的表项），列表只在真正新 PID 时追加节点。
- `DEBUGGER_TABLE_ENTRY` 新增 `RegisteredProcess`：登记时刻的 `EPROCESS` 实例地址，
  仅作身份标记，探测后立即 `ObDereferenceObject`，不延长进程对象生命周期。
- 新增 `PurgeStaleDebuggerList()`：两阶段清理（阶段 1 不持锁做
  `PsLookupProcessByProcessId` 存活/实例同一性探测；阶段 2 持 `FAST_MUTEX` 摘链并
  `free_pool`）。摘链时带 `staleLiveInstances` 参与判定，避免“同 PID 新旧两个表项”
  时误删身份正确的新表项；遍历采用“先取后继再摘链释放”。
- `InitDebuggerInfo` 在灌入本次登记前调用一次清理；登记时记录 `RegisteredProcess`。
- IRQL：`PsLookupProcessByProcessId` 要求 PASSIVE_LEVEL，唯一调用点
  `IOCTL_LOAD_DEBUGGER_DATA`（METHOD_BUFFERED）与既有 `InitSymbolsTable` 同层级，
  满足该要求；探测期间不持锁。

**连锁影响复核**：`IsDebugger()` 只读 `dwPid` 且在 `g_DebuggerList.Mutex` 内遍历，
不返回表项指针，因此持锁摘链 + `free_pool` 不会产生在途 UAF；
`ReleaseListEntries` 仍是卸载路径的唯一批量释放点，二者不重叠释放同一节点。

**回归检查**：SOURCE-LEVEL（两个驱动均编译通过，模拟器 73 checks / 0 failures）。
Runtime 级验证仍需真实加载驱动后观察调试器重启/复用 PID 场景。

## 十一、下一轮运行必须采集的证据（禁止用猜测替代）

按以下顺序各跑一次，并把 `C:\DbgkSys10.log` / `C:\DbgkSys11.log` 完整保留：

1. 启动 VT → GUI 启动 x64dbg → **只向 x64dbg 注入 Hook64.dll**（`UnrealDbgDll` 始终只加载在 GUI 进程）。
2. x64dbg 打开 `UnrealDbg.vmp.exe`（或加载 `UnrealDbgDll.dll` 的目标进程）。
3. 记录提示框出现的**进程名与时刻**，与日志时间对齐。

需要在这两处日志行上做判定（都是本驱动自己写出的 SOURCE FACT，不是推断）：

```text
[AntiDebug] debug object type indexes: virtual=... standard=...
[AntiDebug] PrepareVirtualDebugTargetProcess process=... pid=...
[AntiDebug] NtQueryInformationProcess class=0x7  callerPid=... callerImage=... target=? handle=...
[AntiDebug] NtQueryInformationProcess class=0x1E callerPid=... callerImage=... target=? handle=...
[AntiDebug] NtQueryInformationProcess class=0x1F callerPid=... callerImage=... target=? handle=...
[AntiDebug] handled class=... target=1 ...
```

判定分支（互斥，不允许跳步）：

- **分支 A：`target=0`** → 目标识别失败。此时 VMProtect 看到的是真实的调试端口/PEB，
  必须回到 `IsVirtualDebugPortObject`（`EPROCESS->DebugPort` 是否指向 `Hvm_DebugObject`）
  与 `SetDebugTargetProcess`（`IsDebugger(当前进程)` 是否命中调试器 PID 表）两条链上定位。
- **分支 B：`target=1` 但 `handled` 未出现** → 判定通过但改写未执行，
  检查查询长度（WOW64 4 字节 / native 8 字节）与 `DebuggerVisibilityPolicy` 的长度门槛。
- **分支 C：`handled class=0x7/0x1E/0x1F` 均已出现但仍弹框** → 说明检测不在
  TitanHide 覆盖的 syscall 面，按 9.4 节逐项排查：
  目标自身内存扫描、调试器进程/窗口名、时间检测、VMProtect 内部自检。
  此时**不能**再声称“与 TitanHide 一致即可解决”。

本节只定义判定路径。在真实运行时数据到手之前，任何“已解决/未解决”的结论都属于猜测。

## 十二、架构事实：调试器工具 / 调试目标（2026-10-17 按源码重新确认）

本节修正此前文档中“UnrealDbgDll 被注入到调试工具”的错误描述。

| 角色 | 真实身份 | 源码依据 |
| --- | --- | --- |
| 控制端 GUI | Delphi `UnrealDbg`（发布为 `UnrealDbg.aes`/`UnrealDbg.vmp.exe`，由 Loader `虚幻调试器.exe` 启动） | `Loader/WinMain/ProcessLaunch.cpp:84`；`UnrealDbg/Forms/Main.pas:285` |
| 控制端代理 DLL | `UnrealDbgDll.dll`，**只加载进 GUI 进程**（不是注入调试工具） | `UnrealDbg/ExternalCall/UnrealDbgDll.pas:44-57` |
| 调试器工具 | 第三方工具（x64dbg / x32dbg / CE），由 GUI 列表选择后 `StartProcess` 启动 | `UnrealDbgDll/StartProcess.cpp:204-327`；`UnrealDbg/Forms/Main.pas:645` |
| 注入对象 | **Hook.dll / Hook64.dll 注入到调试器工具进程**（按工具位数选择） | `UnrealDbgDll/StartProcess.cpp:13-14,236-295` |
| 调试目标 | 第三方工具打开/附加的那个进程（例如 `UnrealDbg.vmp.exe`，或加载 `UnrealDbgDll.dll` 的目标进程） | 驱动侧 `g_DebugProcessList` / `EPROCESS->DebugPort==Hvm_DebugObject` |

驱动侧对应的两组状态：

- `g_DebuggerList`（调试器一侧）：GUI 进程 PID + 第三方工具 PID。
  `IsDebugger()` 只按 PID 查这张表（`Core/DebugEvents/DebugRuntimeEvents.cpp:46-83`）。
- `g_DebugProcessList` / `EPROCESS->DebugPort`（被调试目标）：
  `IsVirtualDebugTargetProcess()` 判定（`Hooks/EptHook/Core/EptHookAntiDebug.cpp:250-268`）。
- 反-反调试 hook（TitanHide 对照）只对“虚拟调试目标”生效；窗口隐藏则额外依赖
  `IsDebugger()`（`Init/Symbolic/InitWin32kbase.cpp:83-107`）。

### FIX-AD-17 互斥不变量：虚拟调试目标不得同时被判定为调试器

- File：`DbgkSysWin10/DbgkApi/Api/DebugProcessRegistry.cpp`、
  `DbgkSysWin11/DbgkApi/Api/DebugProcessRegistry.cpp`、
  `DbgkSysWin10/Core/IoctlHandlers/DebuggerInfoHandlers.cpp`、
  `DbgkSysWin11/Core/IoctlHandlers/DebuggerInfoHandlers.cpp`、
  `DbgkSysWin*/DbgkApi/Api/DebugThreadCreate.cpp`、`DbgkSysWin*/DbgkApi/*` 头文件。
- 根因：旧实现允许同一个进程同时出现在 `g_DebuggerList` 与虚拟调试目标集合。
  当被调试目标自身加载 `UnrealDbgDll` 并执行 `Initialize_F3` 时，它会把自己登记成
  “调试器”；此后 `NewValidateHwnd` 的 `if (!IsDebugger(PsGetCurrentProcess()))`
  不成立，窗口隐藏整段被跳过，VMProtect 的窗口类名扫描即可命中第三方工具窗口。
- 修复：
  1. `RemoveDebuggerListEntry()`：按 `RegisteredProcess`/PID 从调试器表摘除；
  2. attach 路径 `SetDebugTargetProcess()` 成功登记后立即摘除该目标；
  3. create 路径 `DbgkCreateThread` 每次确认虚拟目标时摘除；
  4. `InitDebuggerInfo` 拒绝把“当前已是虚拟调试目标”的 PID 登记成调试器，并写
     `[AntiDebug] refuse debugger registration pid=...` 日志。
- 锁顺序：attach 路径持 `DbgkpProcessDebugPortMutex` → `g_DebugProcessList.Mutex`
  → `g_DebuggerList.Mutex`；不存在反向持锁路径，不会锁倒置。摘链先取后继再释放，
  无在途 UAF（`IsDebugger` 不返回表项指针）。
- 状态：SOURCE-LEVEL FIXED（Win10/Win11 分离实现，均编译通过）。
