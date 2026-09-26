# 20 CE / x64dbg / x32dbg 调试能力全量适配分析与方案

> 目标：让 CE、x64dbg、x32dbg 的**软件 INT3 断点、硬件断点、单步/步进/步过/步出**在本项目上全部可用且行为正确。
> 方法：逐行扫描 + 以三个调试器的**真实源码**与 Intel SDM 为依据，不凭猜测改代码。
> 前置：`docs/00`（总览）、`docs/01`（VT_Driver 逻辑）、`docs/04`（Bug 清单）、`docs/09`（多维度审计）、`docs/12`（0x101 最终结论）、`docs/18`（INT3 0x50）、`docs/19`（WH_MOUSE_LL）。

---

## 0 一句话结论

> 本项目的调试桥**设计是完整且自洽的**：软件 INT3 走「EPT 伪页」隐藏，硬件断点走「DRx → EPT watch → 注入 #DB」仿真，调试事件走「guest Dbgk Hook + 用户态 Hook DLL」。
>
> **本轮已修掉两个最关键的问题**：
> 1. **执行型硬件断点（DR7.RWn=00）被静默丢弃**——而这是 x64dbg 硬件断点的**默认类型**、CE 的 `bptExecute` 类型。表现为「调试器提示设置成功，实际不生效，还会连带清掉同槽位已有断点」。→ **已修**（§6.1）
> 2. **`DR7.GD` 会让处理器在 root 态执行 `MOV DR` 触发 `#DB` → 三重故障蓝屏**。→ **已修**：启用 `MOV_DR_EXITING` 并做 DRx 虚拟化，`GUEST_DR7` 永不携带 GD（KVM `handle_dr` 同构），同时在 VM-exit 序言把物理 DR7 收敛为 `DR7_FIXED_1` 以隔离 guest 断点（§6.2）
>
> 剩余缺口见 §4 G4–G11，均已在 §5 排序。

---

## 1 系统架构与调用通道

### 1.1 四个组件

| 组件 | 位置 | 角色 |
| --- | --- | --- |
| **VT_Driver.sys** | `VT_Driver/`（187 文件 / 约 2.2 万行） | VMX root 态 hypervisor：VMCS、EPT 构建、EPT Hook/Watch、INT3 伪页、VM-exit 分发、VMCALL 服务 |
| **DbgkSysWin10/11.sys** | `DbgkSysWin10/`、`DbgkSysWin11/`（各 60+ 文件） | guest 态内核驱动：Hook 调试子系统（Dbgk）与 win32k，接管/伪造调试事件，转发断点请求到 VT_Driver |
| **Hook.dll / Hook64.dll** | `Hook/` | 注入**调试器进程**，Hook 调试 API 与内存 API，把断点/上下文操作转成 IOCTL |
| **Delphi GUI / Loader** | `UnrealDbg/`、`Loader/`、`CardRegistration/`、`D-encryption*` | 授权、加密、驱动装载、界面（本任务只做逻辑理解） |

### 1.2 两条控制通道

```
① 断点/监视控制面（低频、可阻塞）
   调试器进程 ──Hook.dll──> IOCTL ──> DbgkSysWin10/11.sys ──current_vmcall()──> VT_Driver
                                        (Common/Ring0/Hvm/hypervisor_gateway.cpp 发 VMCALL)

② 命中/事件回传面（VM-exit 内，禁止阻塞）
   VT_Driver 记录 vcpu->breakpoint_detected / breakpoint_debug_register
        ├─ 注入 #DB(1) 给 guest（Globals/GuestWatchState.cpp:inject_single_step）
        ├─ guest Dbgk 把 EXCEPTION_SINGLE_STEP 投给调试器
        └─ Hook.dll 在 WaitForDebugEvent 命中时用 VMCALL_GET_BREAKPOINT 取命中详情
```

### 1.3 VMCALL 命令面（`Common/Shared/VmcallReason.h`，全部 26 条）

| 码 | 名称 | 作用 |
| --- | --- | --- |
| 0/1 | `TEST` / `VMXOFF` | 探活 / 卸载 |
| 2/3/4/5/6 | `EPT_CC_HOOK` / `EPT_INT1_HOOK` / `EPT_RIP_HOOK` / `EPT_HOOK_FUNCTION` / `EPT_UNHOOK_FUNCTION` | 函数级 EPT Hook 生命周期 |
| 7 | `INVEPT_CONTEXT` | 指定上下文刷新 |
| 8/9 | `DUMP_POOL_MANAGER` / `DUMP_VMCS_STATE` | 诊断 |
| 10/11 | `HIDE_HV_PRESENCE` / `UNHIDE_HV_PRESENCE` | 隐藏虚拟化痕迹 |
| 12/13 | `HIDE_SOFTWARE_BREAKPOINT` / `READ_SOFTWARE_BREAKPOINT` | **INT3 伪页安装 / 读取** |
| 14 | `READ_EPT_FAKE_PAGE_MEMORY` | 读伪页内容（回填给调试器） |
| 15/16/17 | `WATCH_WRITES` / `WATCH_READS` / `WATCH_EXECUTES` | **硬件断点 → EPT 写/读写/执行监视** |
| 18 | `WATCH_DELETE` | 删除监视 |
| 19 | `GET_BREAKPOINT` | **取一次性命中断点（消费）** |
| 20 | `INIT_OFFSET` | 下发 ntoskrnl 结构偏移 |
| 21 | `CLEAR_BREAKPOINT_STATE` | 清理残留命中态 |
| 22 | `SYNC_CR3_EXIT_POLICY` | 动态开关 CR3-load exiting |
| 23/24 | `SET/GET_SOFTWARE_BREAKPOINT_BUFFER` | 伪页缓冲区 |
| 25 | `PEEK_BREAKPOINT` | 窥探命中（不消费） |

> **关键观察：命令面里没有任何 DRx 虚拟化命令。** 硬件断点完全依赖「转成 EPT watch」这一条路，VMM 不接管 guest 的 DRx。

---

## 2 调试能力现状矩阵

| 能力 | 状态 | 实现位置 |
| --- | --- | --- |
| **INT3 软件断点隐藏** | ✅ 完整 | `Ept/Watch/EptWatchBreakpointInstall.cpp:22 set_hide_software_breakpoint()`（`bpType=3`，EPT 伪页）；`VmExit/VmExitException.cpp:171-216`（MTF 收尾时整页同步 + 重写同页 0xCC） |
| INT3 读回（调试器读内存看到原字节） | ✅ | `Hook/.../HiddenInt3MemoryRead.cpp`；`VMCALL_READ_SOFTWARE_BREAKPOINT` |
| INT3 写回（调试器写 0xCC / 恢复原字节） | ✅ | `Hook/.../HiddenInt3MemoryWrite.cpp`、`HiddenInt3WriteRemoval.cpp` |
| **硬件断点：写型（RW=01）** | ✅ | `Hook/DebugBreak/Core/DebugRegisterHelpers.cpp:82` → `VMCALL_WATCH_WRITES` |
| **硬件断点：读写型（RW=11）** | ✅ | 同上 `:86` → `VMCALL_WATCH_READS` |
| **硬件断点：执行型（RW=00）** | ❌→✅ **本轮修复** | 原 `:79` `return false`（静默丢弃）；现映射到 `VMCALL_WATCH_EXECUTES` |
| 硬件断点隐藏（DRx 不落硬件） | ✅ | `Hook/DebugBreak/Core/ThreadContextHooks.cpp:100-105` 把 Dr0-3/Dr6 清零、Dr7=0x400 再按需回填 |
| DR6 合成（告诉调试器哪个槽命中） | ✅ | `ThreadContextHooks.cpp:150-198`；命中来源 `VMCALL_GET_BREAKPOINT` |
| **命中 → #DB 注入** | ✅ | `Globals/GuestWatchState.cpp:5 inject_single_step()`（`EXCEPTION_VECTOR_SINGLE_STEP` + DR6 快照） |
| **单步 / 步进（TF）** | ⚠️ 仅原生透传 | 无 VMM 参与：调试器经 `SetThreadContext` 置 `RFLAGS.TF`，硬件在 guest 内产生 #DB。VMM 只在 `DebugEventHitRecords.cpp:54 CaptureSingleStepDebugState()` 里**记录** |
| **步过 / 步出** | ⚠️ 依赖调试器 | x64dbg 用「临时 INT3（`UE_SINGLESHOOT`）」+ TF；本项目的 INT3 隐藏链可承载，但未针对「一次性断点」做显式建模 |
| 调试事件接管 | ✅ | `DbgkSysWin10/11/Hooks/EptHook/`（Hook `DbgkpQueueMessage`/`DbgkpSendApiMessage`/`NtRemoveProcessDebug`/`NtDebugContinue` 等） |
| 断点事件在附加/创建两条路径的区分 | ✅ | `Hook/DebugEvent/Core/WaitForDebugEventHook.cpp:27-118` |
| **MOV DR 虚拟化** | ✅ 本轮启用 | `Vmcs/VmcsControls.cpp:109` 无条件 `mov_dr_exiting = true`；`VmExit/VmExitDebugRegisters.cpp` 重写为影子读写 |
| **`DR7.GD` 宿主防护** | ✅ 本轮修复 | `GUEST_DR7` 永不携带 GD（`Processor/drx.h:sanitize_guest_dr7`）；VM-exit 序言把物理 DR7 收敛为 `DR7_FIXED_1`（`ASM/vm-exit.asm:49-58`） |
| **#DB / #BP 异常拦截** | ❌ 未启用 | `Vmcs/VmcsControls.cpp:214-235` 异常位图**全 0**（GD 由 MOV DR handler 单独仿真，不依赖异常位图） |
| guest 故障处理 | ⚠️ 一律蓝屏 | `VmExit/VmExitException.cpp:265 vmexit_triple_fault_handler()` → `vmx_fatal_error` → `KeBugCheckEx` |

---

## 3 三个调试器的真实行为（取证）

### 3.1 x64dbg / x32dbg（源码：`F:\001\VT\中间目录\x64dbg-development`）

**硬件断点**（`src/dbg/commands/cmd-breakpoint-control.cpp:400`）：

```cpp
TitanHardwareBreakpointType type = UE_HARDWARE_EXECUTE;   // ← 默认就是执行型
if(argc > 2) {
    switch(*argv[2]) {
    case 'r': type = UE_HARDWARE_READWRITE; break;
    case 'w': type = UE_HARDWARE_WRITE;     break;
    case 'x': break;                        // 默认
    }
}
```

**TitanEngine 写 DR7**（`src/third_party/TitanEngine/TitanEngine/TitanEngine.Breakpoints.cpp:640-745`）：

```cpp
case UE_HARDWARE_EXECUTE:  hwbpSize = SIZE_1; hwbpType = TYPE_EXECUTE;   // RW=00, LEN=00
case UE_HARDWARE_WRITE:                        hwbpType = TYPE_WRITE;    // RW=01
case UE_HARDWARE_READWRITE:                    hwbpType = TYPE_READWRITE;// RW=11
dr7.HWBP_MODE[i] = MODE_LOCAL; dr7.HWBP_SIZE[i] = hwbpSize; dr7.HWBP_TYPE[i] = hwbpType;
SetContextDataEx(hThread, UE_DR7, dr7uint(&dr7));   // 先写 DR7
SetContextDataEx(hThread, IndexOfRegister, bpxAddress);
```

**单步 / 步过 / 步出**（`TitanEngine.Debugger.Control.cpp:39-95`）：

```cpp
void StepInto(cb) {
    if (PUSHF)                 StepOver(cb);
    else if (POP SS || MOV SS) SetBPX(rip+len, INT3 + UE_SINGLESHOOT, cb);  // 临时 INT3
    else { ...; myDBGContext.EFlags |= UE_TRAP_FLAG; SetThreadContext(...); } // ← TF
}
void StepOver(cb) {
    if (CALL || REP || PUSHF) SetBPX(rip+len, INT3 + UE_SINGLESHOOT, cb);   // 临时 INT3
    else                      StepInto(cb);
}
void StepOut(cb) { StepOver(StepOutStepCallBack); }
```

**上下文标志**（`Global.Debugger.cpp:50-51`）：

```cpp
// Include CONTEXT_DEBUG_REGISTERS so breakpoint steps never erase them.
DWORD flags = CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
```

> **结论 1**：x64dbg 硬件断点默认执行型；`bph`/GUI 三种类型都要支持。
> **结论 2**：**x64dbg 的单步/步过不使用执行型 DRx**，而是 `TF` + `UE_SINGLESHOOT` 临时 INT3。原代码注释「x64dbg 会用它做内部步过」**与现行实现不符**，是本次修复的依据之一。
> **结论 3**：TitanEngine 的每次上下文读写都带 `CONTEXT_DEBUG_REGISTERS`，因此 `ThreadContextHooks` 的 `IsCurrentHardwareBreakpointState` 幂等守卫是**必经路径**，必须保证它在新类型下也成立。

### 3.2 CE（源码：`F:\001\VT\中间目录\cheat-engine`）

**断点类型 → DR7 编码**（`Cheat Engine/debughelper.pas:1025-1049`）：

```pascal
Debugregistermask := 0;
case breakpoint.trigger of
  bptWrite:  Debugregistermask := $1 or Debugregistermask;   // RW=01
  bptAccess: Debugregistermask := $3 or Debugregistermask;   // RW=11
end;                                                          // bptExecute → RW=00
case breakpoint.size of
  2: Debugregistermask := $4 or ...;  4: Debugregistermask := $c or ...;  8: Debugregistermask := $8 or ...;
end;
Debugregistermask := (Debugregistermask shl (16 + 4 * breakpoint.debugRegister));
Debugregistermask := Debugregistermask or (1 shl (breakpoint.debugregister * 2)); // Ln
Debugregistermask := Debugregistermask or (1 shl 10);                             // DR7 保留位
clearmask := (($F shl (16 + 4 * breakpoint.debugRegister)) or (1 shl (breakpoint.debugRegister*2))) xor $FFFFFFFF;
```

**写入方式**：`currentthread.context.Dr0..Dr3 := address; context.Dr7 := (Dr7 and clearmask) or Debugregistermask; currentthread.setContext(cfDebug);` → 走 `SetThreadContext`，**每个线程逐个设置**（`debughelper.pas:1097-1160`）。

> **结论 4**：CE 的 `bptExecute` 同样是 RW=00；「查找访问」用 RW=11。CE 也依赖 `SetThreadContext`，与 x64dbg 走同一条桥。
> **结论 5**：CE 会设 `DR7[10]=1`（保留位），桥侧 `Dr7 = 0x400` 的基线与之相容。

### 3.3 官方文档依据

| 结论 | 依据 |
| --- | --- |
| 执行断点必须 LEN=00 | Intel SDM Vol3B 17.2.4（DR7 的 LENn 对 RW=00 保留为 00） |
| GD 会在**访问任何调试寄存器**时产生 #DB，且**进入 #DB 处理程序时硬件清 GD** | Intel SDM Vol3B 17.2.1；SDM 原文：*"The processor clears the GD flag upon entering to the debug exception handler, to allow the handler access to the debug registers."*（Vol3B 9977 行） |
| `Save debug controls` 决定 VM-exit 是否把 DR7 存入 GUEST_DR7 | Intel SDM Vol3C 表 27-14 bit2 |
| `Load debug controls` 决定 VM-entry 是否从 GUEST_DR7 载入 DR7 | Intel SDM Vol3C 表 26-*（5238 行附近） |
| MTF 与 guest 的 TF/DR7 单步相互独立、guest 不可见 | Intel SDM Vol3C 25.5.2 |
| KVM 用 `MOV_DR_EXITING` + `handle_dr` 处理 GD：`dr7 & DR7_GD → kvm_queue_exception_p(vcpu, DB_VECTOR, DR6_BD)` | `linux-vmx.c:5761-5786` |
| KVM 在投递 #DB 时清 `DR7_GD`（一次性语义） | `linux-v6.12-kvm-x86.c:10369-10371` |

---

## 4 缺口清单（按严重度）

### G1【严重·功能性】执行型硬件断点被静默丢弃 — **本轮已修**

- 位置：`Hook/DebugBreak/Core/DebugRegisterHelpers.cpp:79`（原 `case WATCH_EXECUTION_ONLY: return false;`）。
- 现象链：
  1. `SyncHardwareBreakpointsFromContext()` 中 `DecodeDebugRegisterCommand(0, ...)` 返回 false → `desired[i]` 保持 false；
  2. 删除循环把该槽位已有断点判为 stale 并 `DeleteBreakpointFromDriver()` → **连带清掉同槽位已生效的断点**；
  3. `allSynced` 仍为 true → `NewSetThreadContext` 返回 TRUE → **x64dbg 认为设置成功**。
- 影响：x64dbg 默认硬件断点（执行型）完全不可用且静默失败；CE 的 `bptExecute` 同样失效。
- 依据：§3.1 结论 1/2、§3.2 结论 4；VMM 侧 `VmcallDispatch.cpp:401` 已完整接线。
- 修复：映射到 `VMCALL_WATCH_EXECUTES`（`EPTW_EXECUTE`）。

### G2【严重·必崩】`DR7.GD` → root 态 `MOV DR` 触发 #DB → 三重故障蓝屏 — **本轮已修**

- 位置：`VT_Driver/ASM/vm-exit.asm:50-59`（保存 DR0-3/DR6）、`:121-130`（恢复 DR0-3/DR6）。
- 机理（SDM 17.2.1 + Vol3C 27.5.1）：
  - VM-exit **没有** host DR7 字段，`Save debug controls` 只把 DR7 存入 GUEST_DR7；因此**root 运行期的物理 DR7 = guest 的 DR7**。
  - `mov_dr_exiting = false`（原 `VmcsControls.cpp:110`）→ guest 的 `MOV DR7` 直接写物理 DR7，GD 位落地。
  - 下一次 VM-exit 的汇编序言执行 `mov rax, dr0` → **GD=1 触发 #DB** → root IDT 路径（`Tables/idt.cpp`）→ 三重故障 → `KeBugCheckEx`。
- 触发条件：目标进程（反调试）执行 `MOV DR7` 置 GD=1。**确定性触发，非偶发。**
- 为什么不能「在汇编里先清 GD」：SDM 17.2.1 规定 GD=1 时**任何** `MOV DR` 都先产生 #DB，`mov dr7, rax` 本身就会先触发，无法自救。
- 依据：`docs/09` B7 / P1-4 已记录；SDM Vol3B 9977 行（*"The processor clears the GD flag upon entering to the debug exception handler"*）/10334 行。
- 修复：见 §6.2。

### G3【中·架构】`MOV DR` 未虚拟化，`vmexit_mov_dr_handler` 是死代码 — **本轮已修**

- 位置：`Vmcs/VmcsControls.cpp:108-113`；`VmExit/VmExitDebugRegisters.cpp` 全文。
- 影响：guest 的 DRx 完全落在物理寄存器上，VMM 无法感知/干预 → 无法实现「guest 自身 DRx 与调试器 DRx 的隔离」，也无法根除 G2。
- 附注（原实现即便启用也有的缺陷）：它用 `__writedr()/__readdr()` 直接操作物理寄存器，而 `vm-exit.asm:121-130` 会用 VM-exit 序言保存的旧值覆盖物理寄存器，**handler 的写入会丢失**。正确做法是写 `vcpu->vmexit_info.guest_registers->drN`（汇编收尾再从该结构写回物理寄存器）。→ 本轮已按此重写。

### G11【低·残留】VMXOFF 路径的 DR7 使能窗口

- 位置：`VmExit/VmExitCommon.cpp:224 __writedr(7, state.guest_dr7)`。
- 说明：`PrepareGuestForVmxoff` 在 ASM 收尾的 `mov dr0..dr6` **之前**把物理 DR7 恢复成 guest 的完整值（含 Ln/Gn 使能）。因此 VMXOFF 路径上，收尾那 ~10 条指令是在「guest 断点使能已打开」的状态下执行的。要触发需 guest 的断点地址恰好命中驱动自身的代码地址（数据断点则要命中宿主栈地址），实际不可能命中；且该路径仅在驱动卸载时走。保留现状，不改。
- 另：KVM 的 host DR7 隔离已由本轮在 VM-exit 序言实现（`DR7_FIXED_1`），主路径（VMRESUME）已完全隔离。

### G4【中·覆盖度·保留现状】硬件断点仅在「用户态 + IF=1 + CR8=0」上报

- 位置：`Ept/Watch/EptWatchEvents.cpp:264-281`。
- 门闸：`canBreak = (CR8 == 0) && rflags.interrupt_enable_flag`，并要求 `!kernelmode` 与 `interruptibility[0] == 0`（非 STI/MOV SS 阻塞窗口），全部满足且 `LoopUserMode` 非零才置 `isBp` → 才注入 #DB。
- **影响范围已核实（重要）**：
  - 该门闸**只作用于 EPT-watch 式硬件断点**（`bpType == 1`，即 `BREAKPOINT_RECORD` 桥接来的 DRx 断点）。
  - **INT3 软件断点不受影响**：走 `bpType == 3` 分支（伪页换入 → guest 内原生 `#BP`），完全绕过本门闸（`EptWatchEvents.cpp:233-244`）。因此 x64dbg/CE 的 INT3 断点在内核态也能命中。
  - 用户态硬件断点（x64dbg/CE 的常规用法）满足全部条件 → 正常工作。
- **保留原因（不改）**：`isBp` 为真后走的是「MTF 收尾 → `inject_single_step()` 注入 #DB」路径，而 `inject_single_step()` 需要 `GuestCurrentThreadCid()` 并能安全注入事件。在 `IF=0` / `CR8>0` / 内核态下注入事件会落到来宾不可预期的位置（可能是临界区），风险高于收益；且 x64dbg 调试内核驱动属少数场景。
- 若后续确需支持内核态硬件断点，正确做法是**解耦「记录」与「投递」**：命中时无条件记录 `breakpoint_detected`，投递延迟到可投递时机（复用已有的 NMI-window 机制：`VmExitException.cpp:25 vmexit_nmi_window_handler`）。依据：jonomango-hv 的 `queued_nmis` + `nmi_window_exiting`、HyperDbg 的 `PendingExternalInterrupts[]` + interrupt-window exiting（见 `docs/13` §4 范式 3）。

### G5【中·正确性】单步/步过状态捕获只在「附加」路径执行 — **本轮已修**

**先确认现状（逐环核对，结论：单步链路本身是通的）**

| 环节 | 实现 | 结论 |
| --- | --- | --- |
| 步进（StepInto） | TitanEngine 置 `EFlags \|= UE_TRAP_FLAG` → `SetThreadContext`（只带 `CONTEXT_CONTROL`）→ 硬件在 guest 内产生 `#DB` | ✅ 纯 guest 行为，无需 VMM 参与 |
| 步过/步出（StepOver/StepOut） | `SetBPX(rip+len, INT3 + UE_SINGLESHOOT)` 下**临时 INT3** → 命中后调试器写回原字节删除 | ✅ 复用既有隐藏 INT3 链路（`HiddenInt3MemoryWrite.cpp` → `RemoveHiddenInt3BreakpointsForWrite`） |
| 单步状态捕获 | `Hook/DebugEvent/Core/WaitForDebugEventHook.cpp` 在 `EXCEPTION_SINGLE_STEP` 时调 `CaptureSingleStepDebugState` | ❌ **只在 `else`（附加）分支调用** |
| DR6 合成 | `Hook/DebugBreak/Core/ThreadContextHooks.cpp:182-198`：条件 1 → `DR6.B0-B3`；条件 2 → `DR6.BS` | ✅ 依赖上一步写入的 `DEBUG_HIT_RECORD` |

**缺陷**：`g_process_info.isCreate = TRUE` 由 `Hook/HookCallSet/Hooks/ProcessApiHooks.cpp:25`（`NewNtCreateUserProcess`）设置，即 **x64dbg 的 File → Open 走的创建路径**。该路径下 `CaptureSingleStepDebugState` 从不执行 → 纯 `TF` 单步拿不到 `DR6.BS`，调试器读到的 DR6 与附加路径不一致（附加路径给 `1<<14`，创建路径给 `0`）。只有当 `activeHardwareBreakpointCount > 0` 时，`NewGetThreadContext` 的 `VMCALL_PEEK_BREAKPOINT` 探测路径才会兜底，且只对「硬件断点命中」有效，对纯单步无效。

**修复**：把单步状态捕获**提升到创建/附加分支之前**，两条路径统一执行（`WaitForDebugEventHook.cpp`）。

**保留项（已核对为设计取舍，不改）**：

1. `ThreadContextHooks.cpp:105` 返回 `Dr7 = 0x400` 基线再按 `BreakpointList` 回填 Ln/RW/LEN —— **LE/GE 位被丢弃**。这两位置控制精确断点，而 EPT watch 模型无法表达精确断点，x64dbg/CE 也都不使用，故丢弃是自洽的。
2. 临时 INT3 与用户手动断点共用 `INT3BreakpointList`，没有独立的「一次性」标记 —— 但 `UE_SINGLESHOOT` 的删除由调试器主动写回原字节触发，走的是同一条已验证的删除路径，无需额外建模。
3. **`TF` 单步与 EPT watch 注入 #DB 可能叠加**：若调试器在带执行型硬件断点的指令上单步，该指令先触发 EPT violation → MTF → `inject_single_step()` 注入 #DB；此时 guest 的 `RFLAGS.TF` 仍为 1。按 SDM 25.5.2，MTF VM-exit 优先于 debug-trap，因此不会丢事件，但**是否会产生一次额外的单步事件需要上机确认**（见 §7.4）。

### G6【中·设计】`#DB` / `#BP` 未被拦截（异常位图全 0）

- 位置：`Vmcs/VmcsControls.cpp:214-235`。
- 影响：
  - 无法在 VMM 侧统一接管硬件断点命中（现依赖 guest Dbgk 投递），调试器绕开 Dbgk 的场景（如 VEH 自处理、`KiDebugRoutine` 被替换）不可控；
  - INT3 的隐藏完全靠 EPT 伪页，一旦 guest 直接读**物理内存**（DMA/驱动直读）仍可看到真实字节（这是伪页方案固有能力边界）。

### G7【已解决·复核】注入静默丢弃 —— 当前实现已改为 FIFO 排队

- `docs/09` B6 引用的 `HypervisorRoutines/HypervisorInterrupts.cpp:30-33` 现已位于**被注释掉的旧函数**内。
- 当前实现（`HypervisorInterrupts.cpp:51-61`）在检测到已有 pending 事件时调用 `hv::queue_pending_vm_entry_event(...)`，并由 `VmExitDispatcher.cpp:258 drain_pending_vm_entry_event()` 与 `:315 ensure_pending_vm_entry_event_progress()`（MTF 强制下一次退出）完成排空。
- 结论：**该缺口在当前代码中已不存在**，`docs/09` 的对应条目应视为过时。保留本节以记录复核结果。

### G8【低·硬编码·需作者确认】`ProtectDrxX64` 的 `EFlags = 0x229`

- 位置：`DbgkSysWin10/11/Protect/Thread/ProtectDrxX64.cpp:102`（Win10/Win11 同构）。
- 代码上下文：

```cpp
if (DebugProcess->Context.initializated)
{
    //ThreadContext->EFlags = DebugProcess->Context.EFlags;   // ← 被注释掉的原始语义
    ThreadContext->EFlags = 0x229;                            // ← 现行硬编码
    DbgPrint("DebugProcess->Context.EFlags: %x\n", ThreadContext->EFlags); // 每次调用都打印
}
```

- **作用域澄清（重要）**：分支条件是 `IsDebugTargetProcess((_EPROCESS*)PsGetCurrentProcess(), ...)`，而 `PsGetCurrentProcess()` 是**发起调用**的进程。x64dbg/CE 调用 `GetThreadContext(目标线程)` 时，当前进程是**调试器**，不是目标 → **该分支对 CE/x64dbg 的常规调用不生效**。它只作用于「调试目标读取自己线程上下文」这一反反调试场景（注释自述为「处理 pubg 的硬件断点占坑」）。
- 因此：**不影响本轮的 CE/x64dbg 适配**，故本轮不动它。
- 但有两处客观缺陷值得作者确认后处理：
  1. `0x229` 会把 CF/AF 强制为 1、ZF/SF/PF/OF 强制为 0，破坏被调试目标读到的真实标志位；
  2. 每次调用都执行 `DbgPrint`，在目标自检热点路径上是明显的性能与日志噪声。
- 建议：恢复被注释掉的原始语义（返回 `DebugProcess->Context.EFlags` 影子），并删除该 `DbgPrint`。**需作者确认是否仍需要针对特定游戏的占坑行为。**

### G9【中·策略】guest 故障升级为宿主蓝屏

- 位置：`VmExit/VmExitException.cpp:265`、`VmExit/VmExitCommon.cpp`（`vmx_fatal_error` → `KeBugCheckEx`）。
- 影响：**调试场景下「被调试程序崩溃」是常态**，把它升级成宿主蓝屏不可接受。对照 HyperPlatform/hvpp/SimpleVisor 均区分「初始化期/guest 侧」与「宿主侧不可恢复」。

### G10【低·性能】`Cr3ScanPhysicalMemory` 全物理内存扫描

- 位置：`DbgkSysWin10/11/Cr3/Cr3Resolver.cpp`（最后手段，O(物理内存/4KB)）。
- 影响：32–64GB 机器上数百万次迭代 + 每次 `MmCopyMemory`，在 IOCTL 上下文里是长停顿；`docs/18` §10 已建议预筛。

---

## 5 修复优先级

| 序 | 缺口 | 风险 | 是否需实机验证 | 状态 |
| --- | --- | --- | --- | --- |
| 1 | G1 执行型硬件断点 | 低（仅用户态） | 建议 | **已修** |
| 2 | G2 `DR7.GD` 蓝屏 | 高（VMCS + 汇编） | **必须** | **已修** |
| 3 | G3 `MOV DR` 虚拟化 | 高（与 G2 同源） | **必须** | **已修** |
| 4 | G7 注入静默丢弃 | 中 | 建议 | **复核后为已解决**（当前实现已改为 FIFO 排队，`docs/09` B6 已过时） |
| 5 | G5 单步/步过状态捕获 | 中 | 建议 | **已修** |
| 6 | G4 内核态硬件断点 | 中 | 建议 | 保留现状（有依据，见 §4 G4；INT3 不受影响） |
| 7 | G9 guest 故障分层 | 中 | 建议 | 待办（需设计 guest reset 仿真，KVM `kvm_emulate_triple_fault` 为参考） |
| 8 | G6 `#DB/#BP` 拦截 | 高 | **必须** | 待评估（现有伪页方案已覆盖 INT3，暂不需要） |
| 9 | G8 `EFlags` 硬编码 | 低 | 否 | 待作者确认（**已确认不影响 CE/x64dbg**，见 §4 G8） |
| 10 | G10 CR3 扫描预筛 | 低 | 否 | 待办 |
| 11 | G11 VMXOFF 路径 DR7 使能窗口 | 极低 | 否 | 已记录，保留现状 |

---

## 6 已实施方案

### 6.1 G1：执行型硬件断点桥接（本轮已改）

**改动 1**：`Hook/DebugBreak/Core/DebugRegisterHelpers.cpp`

```cpp
case WATCH_EXECUTION_ONLY:
    *command = VMCALL_WATCH_EXECUTES;
    return true;
```

**改动 2**：`Hook/DebugBreak/Core/HardwareBreakpointSync.cpp` —— 更新注释，明确现在只有 `RW=10`（I/O 断点，已废弃）不参与同步。

**连带确认（无需改动）**：

- `IsCurrentHardwareBreakpointState()` 已比较 `rwCodes[]`，执行型下 `rwCode=0` 与 `BreakpointList` 中 `debugRw=0` 一致 → 幂等守卫成立 → TitanEngine 每次带 `CONTEXT_DEBUG_REGISTERS` 的上下文往返不会误删断点。
- `NewGetThreadContext()` 已按 `debugRw/debugLen` 回填 `Dr7` 的 RW/LEN 位（`:163-166`），执行型下写回 `RW=00` 正确。
- VMM 侧 `VmcallDispatch.cpp:401` → `SetBreakpoint(..., EPTW_EXECUTE)` → `EptWatchPageInstall.cpp:16-19 set_ept_watch()` 清 EPT 条目 execute 位；命中判定 `EptWatchEvents.cpp:17-20` 匹配 `execute_access`。

**注意**：`VMCALL_WATCH_EXECUTES` 此前**无任何调用者**，即 VMM 的执行监视分支属**从未被执行过的代码路径**。启用后必须按 §7 逐项验证。

### 6.2 G2 + G3：`MOV DR` 虚拟化与 `DR7.GD` 消除（本轮已实施）

**为什么不能用「汇编里先清 GD」绕过**：SDM 17.2.1 —— GD=1 时**任何** `MOV DR` 都先产生 #DB。因此 `mov dr7, rax`（想清 GD）本身就会触发 #DB，无法自救。

**为什么可以无条件开启 `MOV_DR_EXITING`（性能前提已核实）**：

| 事实 | 依据 |
| --- | --- |
| Windows 仅在线程 `ActiveDR7` 置位时才在线程切换保存/恢复 DRx | `KTHREAD.Header.DebugActive` 位域含 `ActiveDR7`：`DbgkSysWin11/ntos/inc/10.0.19041.4291/ntkrnlmp.h:3466` |
| 本项目的用户态桥已把调试器的 DRx 从硬件剥离，目标线程 `ActiveDR7` 保持 0 | `Hook/DebugBreak/Core/ThreadContextHooks.cpp:100-105`（Dr0-3/Dr6 清零、Dr7=0x400） |
| KVM 亦采用同一模型（`CPU_BASED_MOV_DR_EXITING` 默认清除，仅在 guest 真用 DRx 时开启） | `linux-vmx.c:4659`、`:5795`、`:5828`（`vmx_sync_dirty_debug_regs` / `handle_dr`） |

→ 常态下 guest 不会产生 `MOV DR`，不会出现退出风暴；只有「调试目标自身设置 DRx」这种本来就需要拦截的场景才会产生退出。

**已实施内容**：

1. `Processor/drx.h`：新增 DR7 位掩码与两个归一化函数
   - `sanitize_guest_dr7(v)` = 保留 guest 可写位 + bit10，**剔除 GD**（写入 `GUEST_DR7` 用）。
   - `normalize_guest_dr7_shadow(v)` = 保留 GD 的 guest 可见值（影子用）。

2. `Vmcs/VmcsControls.cpp`：`primary_controls.mov_dr_exiting = true;`（去掉 `_MINIMAL` 分叉）。

3. `Globals/GlobalsVmmContextTypes.h`：`__vcpu` 新增 `unsigned __int64 guest_dr7_shadow;`。

4. `Vmcs/VmcsGuest.cpp`：`vcpu->guest_dr7_shadow = normalize_guest_dr7_shadow(__readdr(7));`，`GUEST_DR7 = sanitize_guest_dr7(...)`。

5. `VmExit/VmExitDebugRegisters.cpp` 重写（KVM `handle_dr` 同构）：
   - DR4/DR5 → `#UD`（KVM 的 `kvm_require_dr` 同序）。
   - `guest_dr7_shadow & GD` → 清 GD + `vmwrite(GUEST_DR7, sanitized)` + 注入 `#DB`（DR6 叠加 `BD` 与固定位），**不推进 RIP**（fault 型，SDM 17.2.1）。
   - 写路径：DR0-3/DR6 写 `guest_context`（**不**用 `__writedr`，否则被汇编收尾覆盖）；DR7 写影子 + `GUEST_DR7`。
   - 读路径：DR0-3/DR6 读 `guest_context`，DR7 读**含 GD 的影子**（保证 `MOV DR7` 读回语义）。

6. `ASM/vm-exit.asm`（序言，debug registers 段首）：`mov rax, dr7` → `and eax, 400h` → `mov dr7, rax`。
   - 作用：root 执行期间物理 DR7 只有 `DR7_FIXED_1`，清除 guest 的 Ln/Gn 使能，避免 root 命中 guest 的 DRx 断点地址触发 #DB（KVM 的 `set_debugreg(DR7_FIXED_1, 7)` 同款做法，见 `linux-kvm-x86.c:8321-8327`）。
   - 无需保存/恢复：VM-entry 会用 `GUEST_DR7` 重新载入 guest 的 DR7。

**语义等价性核对**（逐条）：

| 场景 | 硬件行为 | 本实现行为 |
| --- | --- | --- |
| guest 写 DR0-3 | 生效 | 影子生效，收尾写回物理寄存器 ✅ |
| guest 写 DR6 | 仅可写位生效 | 归一化后写回 ✅ |
| guest 读 DR7 | 返回含 GD 的值 | 返回 `guest_dr7_shadow`（含 GD）✅ |
| guest 写 DR7 置 GD，随后 MOV DR | 该 MOV DR 先 #DB，硬件清 GD（一次性） | 注入 #DB + 清影子 GD，不推进 RIP ✅ |
| guest 自身 DRx 断点命中 | 硬件置 DR6 | 硬件置物理 DR6，prologue 存入 `guest_context->dr6`，handler 读回 ✅ |
| guest 访问 DR4/DR5 | CR4.DE=0 时 #UD | 注入 #UD ✅ |

**回退开关建议**：若上机发现 `MOV DR` 退出频率异常（`vcpu->vmexit_telemetry.reasonCount[EXIT_REASON_MOV_DR]`），可参照 KVM 改为条件开启（仅在 `guest_dr7_shadow` 含使能位时打开）；但**不能**直接关掉，否则 G2 立即复现。

### 6.3 G4：内核态/关中断硬件断点

- 现状门闸（`EptWatchEvents.cpp:264-281`）的本意是「不在不可重入上下文注入事件」。
- 建议：把「是否上报」与「何时投递」解耦 —— 命中时无条件记录到 `vcpu->breakpoint_detected`，投递改为**延迟到可投递时机**（NMI-window 已有同类机制可复用：`VmExitException.cpp:25 vmexit_nmi_window_handler`）。
- 依据：jonomango-hv 的 `queued_nmis` + `nmi_window_exiting`；HyperDbg 的 `PendingExternalInterrupts[]` + interrupt-window exiting。

---

## 7 验证清单（上机）

### 7.1 G1 执行型硬件断点（本轮改动）

1. x64dbg 载入目标 → 在目标函数入口 `bph` 或右键「断点 → 硬件断点」（默认执行型）→ 确认状态栏无 "Could not enable hardware breakpoint"。
2. 触发该地址执行 → 确认停在断点、`DR6` 显示正确槽位（B0–B3 之一）。
3. 同一进程再设一个读写型（`bphws addr, r`）→ 确认两者**互不干扰**、都能命中。
4. 删除执行型断点 → 确认读写型仍生效（验证 G1 的「连带清除」已消失）。
5. CE：目标地址右键「找出是什么访问了该地址」→ 选「执行」类型 → 确认命中列表正常。
6. 观察 `VTLoad.log` 是否有 `未处理 EPT violation`、`vmx_fatal_*`、`0x20001` 记录。

### 7.2 G2/G3 回归（已实施，必须上机验证）

1. **GD 不蓝屏**：写一个最小测试程序，用 `SetThreadContext` 置 `Dr7 |= 0x2000`（GD），随后执行任意 `MOV DR0, rax` → **不得蓝屏**；guest 侧应收到 `#DB` 且 `DR6.BD`（bit13）= 1。
2. **GD 一次性语义**：连续两次 `MOV DR0, rax` → 只有第一次应触发 `#DB`（SDM 17.2.1：进入 #DB 处理程序时硬件清 GD）。
3. **DR7 读回**：`MOV rax, dr7` 在写入 GD 后应立即读回 GD=1（影子语义）。
4. **DR0-DR3 往返**：反复 `MOV DRn, rax` / `MOV rax, drn` 读写 → 值应正确往返。
5. **guest 自身硬件断点仍生效**：在目标进程内用 `SetThreadContext` 设一个执行型 DR0 断点（不经调试器桥）→ 应能命中并收到 `#DB`。
6. **退出频率**：观察 `vcpu->vmexit_telemetry.reasonCount[EXIT_REASON_MOV_DR]`（可在 `VMCALL_DUMP_POOL_MANAGER`/诊断路径输出）。空闲系统上应接近 0；若某进程持续刷高，说明该进程自身在用 DRx（属预期），需评估是否需要条件开关。
7. **CR4.DE 边界**：访问 DR4/DR5 应得到 `#UD`（Windows 不启用 CR4.DE）。
8. 观察 `VTLoad.log` 是否有 `vmx_fatal_*`、`0x20001` 记录。

### 7.3 通用

1. `DbgkSysWin10` / `DbgkSysWin11` / `VT_Driver` / `Hook` 全部 Release x64 编译 **0 error / 0 warning**。
2. 三类断点 × 三种调试器（CE / x64dbg / x32dbg）× 两种位数（32/64 位目标）组合冒烟。
3. 单步/步进/步过/步出 各连续执行 100 次，确认无卡死、无事件丢失。
4. 反复附加/脱离 10 次，确认无残留 watch、无 `0x101`、无 `0x50`。

### 7.4 G5 单步/步过（本轮改动，两条路径都要测）

**创建路径**（x64dbg File → Open，走 `NewNtCreateUserProcess` → `isCreate = TRUE`）与**附加路径**（x64dbg Attach / CE Attach）**必须分别验证**：

1. `F7` 步进 100 次 → 每次都能停住；`GetThreadContext` 返回的 `DR6` 应含 `BS`(bit14)。
2. `F8` 步过一条 `CALL` → 应停在 CALL 的下一条指令（临时 INT3 路径），且临时断点在命中后被正确移除（再次 `F8`/`F9` 不重复命中）。
3. `F9` 运行到下一个断点 → 正常。
4. **叠加场景**：在带执行型硬件断点的地址上单步 → 确认只产生一次单步事件，不重复（见 §4 G5 保留项 3）。
5. `StepOut` 从函数内部返回调用者 → 正常。
6. 对比创建/附加两条路径下同一操作的 `DR6` 是否一致（这是本轮修复的回归点）。
7. 用 CE 的「单步」与「步过」各验证一遍。

---

## 8 依据（可溯源）

**Intel / Microsoft**
- Intel SDM Vol3B 17.2.1（GD 语义、进入 #DB 处理程序时硬件清 GD）、17.2.4（DR7 布局与 LENn）、18.2.4（DR7）。
- Intel SDM Vol3C 25.5.2（MTF）、26.5.2（事件注入）、27.5.1（Loading Host State，无 host DR7）、27.5.2、表 27-14（`Save debug controls`）、VM-entry `Load debug controls`。

**调试器真实实现（本机源码）**
- `x64dbg-development/src/dbg/commands/cmd-breakpoint-control.cpp:355-480`（硬件断点默认 `UE_HARDWARE_EXECUTE`）。
- `x64dbg-development/src/third_party/TitanEngine/TitanEngine/TitanEngine.Breakpoints.cpp:640-745`（`SetHardwareBreakPoint` DR7 编码）。
- `x64dbg-development/src/third_party/TitanEngine/TitanEngine/TitanEngine.Debugger.Control.cpp:39-95`（`StepInto` 用 TF；`StepOver`/`StepOut` 用 `UE_SINGLESHOOT` 临时 INT3）。
- `x64dbg-development/src/third_party/TitanEngine/TitanEngine/Global.Debugger.cpp:44-52`（`ContextControlFlags = CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS`）。
- `cheat-engine/Cheat Engine/debughelper.pas:1025-1049`（`bptExecute/bptWrite/bptAccess` → DR7 掩码）、`:1090-1170`（逐线程 `SetThreadContext`）。
- `cheat-engine/Cheat Engine/debuggertypedefinitions.pas:44`（`TBreakpointTrigger = (bptExecute=0, bptAccess=1, bptWrite=2)`）。

**开源 hypervisor**
- KVM：`linux-vmx.c:5761-5786`（`handle_dr`，GD → `kvm_queue_exception_p(vcpu, DB_VECTOR, DR6_BD)`）、`linux-v6.12-kvm-x86.c:1576-1580`（`kvm_set_dr(7, val)`）、`:10369-10371`（投递 #DB 时清 GD）。
- HyperPlatform（热路径禁 API、池耗尽即失败）、DdiMon（MTF 三行紧邻 + `NT_ASSERT`）、hvpp（`interrupt_queue_t`）、HyperDbg（`PendingExternalInterrupts[]` + 窗口重投）、jonomango-hv（`queued_nmis` + `nmi_window_exiting`）—— 见 `docs/13`。

**本仓库**
- `docs/00`、`docs/01`、`docs/04`（B16/B17）、`docs/09`（B6/B7、P0-7、P1-4）、`docs/12`、`docs/13`、`docs/18`、`docs/19`。

---

## 9 本轮实际改动文件

| 文件 | 改动 |
| --- | --- |
| `Hook/DebugBreak/Core/DebugRegisterHelpers.cpp` | **G1**：`WATCH_EXECUTION_ONLY` 由「拒绝」改为映射 `VMCALL_WATCH_EXECUTES`，并补齐依据注释 |
| `Hook/DebugBreak/Core/HardwareBreakpointSync.cpp` | **G1**：更新「不支持的硬件断点类型」注释：现在仅 `RW=10`（I/O 断点）不同步 |
| `Hook/DebugEvent/Core/WaitForDebugEventHook.cpp` | **G5**：单步/硬件断点命中状态捕获提升到创建/附加分支之前，两条路径统一执行 |
| `VT_Driver/Processor/drx.h` | **G2/G3**：新增 DR7 位掩码常量与 `sanitize_guest_dr7` / `normalize_guest_dr7_shadow` |
| `VT_Driver/Globals/GlobalsVmmContextTypes.h` | **G2/G3**：`__vcpu` 新增 `guest_dr7_shadow` |
| `VT_Driver/Vmcs/VmcsControls.cpp` | **G2/G3**：`mov_dr_exiting` 改为无条件 `true` |
| `VT_Driver/Vmcs/VmcsGuest.cpp` | **G2/G3**：`GUEST_DR7` 改为 sanitized，并初始化影子 |
| `VT_Driver/VmExit/VmExitDebugRegisters.cpp` | **G2/G3**：整体重写为影子式 MOV DR 虚拟化 + GD 一次性仿真 |
| `VT_Driver/ASM/vm-exit.asm` | **G2/G3**：序言把物理 DR7 收敛为 `DR7_FIXED_1`，隔离 guest 断点使能 |
| `docs/00-项目总览与架构.md` | 修正「不含硬件断点（DRx）」的过时表述 |
| `docs/20-...适配分析与方案.md` | 本文件（新增） |

**未改动**：`Hook` 以外的任何用户态组件；`DbgkSysWin10/11`；EPT Hook/Watch 链；锁体系。G4/G5/G6/G9 仍待办（§5）。

> **编译验证说明**：本机 MSBuild 被安全策略拦截（与 `docs/18` §10 一致），无法编译。改动集中在枚举常量分支、constexpr 辅助、单一 handler 重写与 3 条 MASM 指令；上机前请按 §7.3 全量重建确认 0 error / 0 warning。
