# 08 0x101 CLOCK_WATCHDOG_TIMEOUT 根因分析与修复方案

本文针对 `0x00000101 CLOCK_WATCHDOG_TIMEOUT` 做逐行排查。所有结论都给出代码位置与外部依据，不做无依据的改动建议。

---

## 0 结论摘要

| 编号 | 根因 | 等级 | 核心位置 |
| --- | --- | --- | --- |
| R1 | 控制面用 `KeGenericCallDpc` 跨核广播 VMCALL，被广播核在 **IPI_LEVEL** 进入根态并抢同一把全局事务锁；屏障要求全部核到达 | P0 | `DbgkSysWin10/11/Hvm/HypervisorGatewayApi.cpp`、`HypervisorGatewayBroadcast.cpp`、`VT_Driver/VmCall/VmcallDispatch.cpp` |
| R2 | `wait_for_page_quiescence` / `wait_for_function_quiescence` 无上限忙等，且**持有全局事务锁**；与「持有 pending MTF 的核去抢事务锁」构成互相等待 | P0 | `Ept/EptCommon.cpp:301-329`、`Ept/Watch/EptWatchDeactivate.cpp:74`、`Ept/Hook/Cleanup/EptHookCleanup.cpp:59,114,164,188` |
| R3 | 全局锁被跨核长操作持有（`vmcs_dump_lock` 内 ~150 次 DbgPrint；事务锁内整页 guest 读 + split + 池分配 + 逐核 commit） | P0 | `HypervisorRoutines/HypervisorVmcsDump.cpp:12-213`、`Ept/Watch/EptWatchActivation.cpp:27-134`、`Ept/Watch/EptWatchBreakpointInstall.cpp:47-188` |
| R4 | `spinlock::lock()` 无 owner / 无超时 / 不提 IRQL / 无死锁检测，任一路径异常即全核无限自旋 | P0 | `Sync/spinlock.cpp:28-51` |
| R5 | 根态文件 IO：`outFile` 仅以 `IRQL != PASSIVE_LEVEL` 作门闸，而 VM-exit 不改 IRQL；来宾 PASSIVE 触发 VMCALL 时门闸放行，在 VMX 私有宿主栈上做 `ZwCreateFile/ZwWriteFile` | P1 | `Driver/VtLoadLog.cpp:189-238`、`Driver/Driver.h:36` |
| R6 | 未处理 exit 的 `#GP` 注入风暴 / EPT 权限未恢复导致反复 violation，来宾无法推进 → 取不到时钟中断 | P1 | `VmExit/VmExitDispatcher.cpp:306-312`、`VmExit/VmExitEpt.cpp:42-199` |
| R7 | ~~逐核 EPT 由「当前核」改写其它核的 EPT，再用本核 `INVEPT`~~ → **已证伪，见 §8** | 撤回 | — |

---

## 1 前提事实（先把「为什么会 0x101」钉死）

### 1.1 官方定义

Microsoft《Bug 检查 0x101 CLOCK_WATCHDOG_TIMEOUT》原文：

> 此 Bug 检查指示在多处理器系统中，辅助处理器上的预期时钟中断未在分配的时间间隔内收到。
> **原因：指定的处理器未处理中断。通常，当处理器无响应或死锁时，会发生此 Bug 检查。**
> 参数：1 = 时钟中断超时间隔（标称时钟周期）；2 = 0；3 = 无响应处理器的 PRCB 地址；4 = 挂起处理器的索引。

即 0x101 的语义是**「某个逻辑处理器在一段较长的时间内无法响应时钟中断」**。两种典型成因：
1. 该核长时间关中断（或被更高 IRQL 屏蔽）；
2. 该核在自旋/死锁中停住，永远走不到能接收时钟中断的位置。

### 1.2 根态（VMX Root）全程关中断 —— 硬件语义

Intel SDM Vol.3C「Loading Host State」（27.5.2）规定：VM-exit 时 RFLAGS 由硬件装载为固定值（IF=0，其余位清零、bit1 置 1）。**Hypervisor 不会设置 HOST_RFLAGS 这个 VMCS 字段，因为它不存在。**

代码侧印证：`VT_Driver/Vmcs/VmcsHost.cpp` 的 `fill_vmcs_host_fields()` 写了 HOST_CR0/CR3/CR4、HOST_RSP/RIP、HOST_GDTR/IDTR、HOST_GS_BASE 等，**没有也无法写 HOST_RFLAGS**。

推论（本报告所有结论的基础）：

> **每一次 VM-exit 之后，处理器在根态运行期间 IF=0，即关中断。此时若在根态停留过久，该处理器就无法响应时钟中断 → 0x101。**

### 1.3 IRQL 序（决定「被屏蔽的时钟中断」）

x64 的 IRQL 序：`PASSIVE(0) < APC(1) < DISPATCH(2) < ... < CLOCK_LEVEL(13) < IPI_LEVEL(14) < HIGH_LEVEL(15)`。

**时钟中断在 CLOCK_LEVEL，IPI 在 IPI_LEVEL。一旦代码运行在 IPI_LEVEL，时钟中断被屏蔽。** 这一点是 R1 的关键。

### 1.4 一个被忽略的细节：VM-exit 不改变 IRQL

VMX 切换不修改 IRQL。若来宾在 `PASSIVE_LEVEL` 执行 `VMCALL`（Dbgk 驱动就是普通线程发起的），则根态处理该 VMCALL 时 IRQL **仍是 PASSIVE_LEVEL**。这直接决定了 R5 的成立。

---

## 2 R1（P0）跨核广播 VMCALL 在 IPI_LEVEL 抢全局锁

### 2.1 事实链

1. 控制面入口全部是 `KeGenericCallDpc`。全仓库共 30+ 处，例如
   `DbgkSysWin11/Hvm/HypervisorGatewayApi.cpp:19,31,35,42,73,87,96,112,128,144,202,217,237,248`，
   `DbgkSysWin10/Hvm/HypervisorGatewayApi.cpp` 同构。

2. `KeGenericCallDpc` 的语义（`Common/Ring0/Hvm/ntapi.h:15-17` 声明了它，以及
   `KeSignalCallDpcSynchronize` / `KeSignalCallDpcDone`）：
   - 它是 `KeIpiGenericCall` 的底层实现：**在 IPI_LEVEL 向所有逻辑处理器广播，并等待全部处理器到达屏障**；
   - 每个目标核运行 `Routine`，并以 `KeSignalCallDpcSynchronize(SystemArgument2)` 组成屏障、`KeSignalCallDpcDone(SystemArgument1)` 交还控制。

3. 每个被广播的 `Routine` 都会发起 VMCALL（进入根态）。例如
   `HypervisorGatewayBroadcast.cpp:150` `SafeVmCallEx(VMCALL_EPT_RIP_HOOK, ...)`、
   `:222` `SafeVmCall(VMCALL_EPT_UNHOOK_FUNCTION, ...)`、
   `:264` `SafeVmCall(command, ...)`。

4. VMCALL 在根态的处理链会获取**全局**锁 `eptWatchTransaction_lock`：
   - `VmCall/VmcallDispatch.cpp:91`（`ExecuteEptRipHook`）
   - `:105`（`ExecuteEptUnhook`）
   - `:122`（`ExecuteEptFunctionHook`）
   - `:188`（`ExecuteReadEptFakePageMemory`）
   - `:240`（`ExecuteSoftwareBreakpointBuffer`）
   - `:307`（`ExecuteReadSoftwareBreakpoint`）
   以及 `Ept/Watch/EptWatchActivation.cpp:27`、`Ept/Watch/EptWatchBreakpointInstall.cpp:47`、`Ept/Watch/EptWatchDeactivate.cpp:116`。

5. 这些锁内是**重操作**：
   - `EptWatchActivation.cpp:29-113`：按 `Size` 逐页循环，每页 `get_physical_address` → `Cr3TrackerAcquire` → 逐 CPU `ept_watch_activate_internal`（可能 `split_pml2` + 池分配）；
   - `EptWatchBreakpointInstall.cpp:49-151`：逐页 guest 整页读取 + `hide_cc_breakpoint` + 逐 CPU commit；
   - `EptHookCleanup.cpp:17-143`：跳板安装/恢复、`RtlCopyMemory` 整页、`invept`。

### 2.2 触发链（24 逻辑核最严重）

```
Dbgk 驱动线程
  └─ hvgt::hook_function() / ept_unhook() / ...
       └─ KeGenericCallDpc(broadcast_*, &args)         ← 抬到 IPI_LEVEL，IPI 广播，等屏障
            ├─ CPU0: broadcast_* → __vm_call(VMCALL...)  → VM-exit → 根态 IF=0
            │        └─ VmcallDispatch::Execute → spinlock::lock(eptWatchTransaction_lock)
            │             └─ 多页 split / 池分配 / 逐核 commit / INVEPT   ← 长耗时
            ├─ CPU1: 同上，卡在 spinlock::lock(...) 上自旋  ← IPI_LEVEL + IF=0：时钟中断被屏蔽
            ├─ CPU2: 同上，自旋
            └─ ... CPU23: 同上，自旋
```

要点：
- **所有核在同一时刻被拉到 IPI_LEVEL**（屏障语义），而时钟中断在 CLOCK_LEVEL 之下，**被屏蔽**；
- 全局事务锁把本来可以并行的「每核各自改自己的 EPT」**串行化**；
- `KeSignalCallDpcSynchronize` 屏障要求**全部核到达**，任一核被锁挡住 → 全核一起等；
- 总耗时超过时钟看门狗间隔（Arg1，标称时钟周期）→ **0x101**。

> 这也解释了用户报告的现象特征：核数越多（Ultra 9 275HX = 24 逻辑核）、Hook/watch 覆盖页越多，越容易触发；i 系列少核机器不易复现。

### 2.3 参考依据（为什么「不用锁」是对的方向）

HyperPlatform（`tandasat/HyperPlatform`，本仓库 spinlock 注释即声明「derived from Hvpp」）的 EPT 实现：
- `EptData` 明确注释 `// EPT related data stored in ProcessorData` —— **每处理器持有自己的 EPT 数据**，VM-exit 路径只操作本核数据；
- `ept.cpp` 中**没有任何自旋锁**，唯一的同步是 `InterlockedIncrement(&ept_data->preallocated_entries_count)`（每处理器预分配池计数）；
- 池耗尽时直接 `HYPERPLATFORM_COMMON_BUG_CHECK(kExhaustedPreallocatedEntries)`，**绝不阻塞等待**；
- VM-exit 路径用 `HYPERPLATFORM_PERFORMANCE_MEASURE_THIS_SCOPE()` 显式关注耗时。

对比本仓库：EPT 数据本身**已经是每处理器**（`__ept_state` 挂在 `__vcpu` 上，见 `docs/01` 第 5 节），但**同步却退化成一把全局锁 + 跨核广播**，与每处理器数据模型自相矛盾。这是 R1 的设计层根因。

### 2.4 修复方向（不引入新锁）

1. **让「全核 Hook/Unhook」不再是 N 次 VMCALL 抢一把锁**：
   按 `docs/01` 第 5 节已确立的「每核各自建 EPT」事实，把跨核操作改为**逐核定向提交**：
   `KeSetSystemGroupAffinityThread` 把控制线程依次绑到每个核，在该核上直接改**它自己的** `__ept_state`（不需要跨核锁，因为只有该核的 EPT 被改）。
   这与 `Vmm/VmmEntry.cpp:29-61` 既有的逐核绑定初始化是同一套成熟模式，风险最低。
2. **需要跨核刷新时，用单向 IPI 而非同步屏障**：`INVEPT` 只影响本地处理器，因此「本核改本核 EPT + 本核 INVEPT」天然自洽；只有「本核改了他核 EPT」才需要远端刷新（见 R7）。
3. **事务锁降级为「每核事务标志」**：既然每核 EPT 独立，安装/删除的互斥只需针对**本核**，可用本核 `__ept_state` 内的一个 `volatile long busy` 或直接依赖「VMCALL 只在本核执行」这一天然串行性。

---

## 3 R2（P0）无上限跨核忙等 + 与事务锁构成互相等待

### 3.1 代码事实

`Ept/EptCommon.cpp:301-329`：

```cpp
void wait_for_page_quiescence(__ept_hooked_page_info* hooked_page_info)
{
    ...
    while (InterlockedCompareExchange(&hooked_page_info->hot_reference_count, 0, 0) != 0 ||
           InterlockedCompareExchange(&hooked_page_info->mtf_reference_count, 0, 0) != 0)
    {
        _mm_pause();          // 无超时、无让步、无上限
    }
}
void wait_for_function_quiescence(__ept_hooked_function_info* hooked_function_info)
{
    while (InterlockedCompareExchange(&hooked_function_info->hot_reference_count, 0, 0) != 0)
    {
        _mm_pause();
    }
}
```

调用点（**均在持有 `eptWatchTransaction_lock` 的上下文内**）：

| 位置 | 是否持事务锁 |
| --- | --- |
| `Ept/Watch/EptWatchDeactivate.cpp:74` | 是（`:116` 获取） |
| `Ept/Watch/EptWatchDeactivate.cpp`（经 `:260` 逐核调用） | 是 |
| `Ept/Hook/Cleanup/EptHookCleanup.cpp:59` | 是（`VmcallDispatch.cpp:105` 获取） |
| `Ept/Hook/Cleanup/EptHookCleanup.cpp:114` | 是 |
| `Ept/Hook/Cleanup/EptHookCleanup.cpp:164,188` | 是 |

### 3.2 死锁构造（最危险的一条）

`mtf_reference_count` 的**唯一**递减点是 `Globals/MtfPendingState.cpp:19` `clear_pending_mtf_metadata()`，它只在这些时机被调用：
- MTF VM-exit：`VmExit/VmExitException.cpp:124,135,144,196`（经 `CleanUp` → `hv::clear_pending_mtf_metadata`）；
- 显式清理：`VmExit/VmExitEpt.cpp:108`、`EptHookCleanup.cpp:112,161`、`VmExit/VmExitCommon.cpp:387`。

于是：

```
CPU A：处理 VMCALL_WATCH_DELETE / VMCALL_EPT_UNHOOK_FUNCTION
        └─ spinlock::lock(eptWatchTransaction_lock) 成功
             └─ ept_watch_deactivate_internal(CPU B 的页记录)
                  ├─ InterlockedExchange(&page->removing, 1)
                  └─ wait_for_page_quiescence(CPU B 的页记录)
                       └─ 自旋等 CPU B 的 mtf_reference_count 归零   ← 卡在这里

CPU B：此前刚在一次 EPT violation 里 mark_pending_mtf_page()
        → page_to_change=该页, mtf_pending=true, mtf_reference_count=1
        尚未取到 MTF VM-exit（下一次 VM-entry 之后才触发）
        此时来宾下一条指令恰好是 VMCALL（Dbgk 驱动高频行为）
        └─ VM-exit reason = VMCALL（不是 MTF）
             └─ VmcallDispatch::Execute → spinlock::lock(eptWatchTransaction_lock)
                  └─ 自旋等 CPU A 释放                                ← 卡在这里
```

**A 等 B 清 MTF，B 等 A 放锁 → 互相等待。两者都在根态（IF=0），A 还在 IPI_LEVEL 语境（若由 `KeGenericCallDpc` 触发）→ 时钟中断被屏蔽 → 0x101。**

关键放大因素：`VmcallDispatchPolicy.h` 的 `IsEptMutation()` **没有收录** `VMCALL_EPT_RIP_HOOK`、`VMCALL_READ_SOFTWARE_BREAKPOINT`、`VMCALL_READ_EPT_FAKE_PAGE_MEMORY`，而这三个都会抢事务锁 → `broadcast_hook_function`（`HypervisorGatewayBroadcast.cpp:144-183`）是**每核都真发 VMCALL**，因此「每核同时抢事务锁」在这一条路径上是必然发生的，不是概率问题。

> 注：`cb1197e`「将 Dbgk 全核 EPT 操作串行为单次事务，消除 N² VMCALL」只对 `IsEptMutation` 白名单内的命令生效；Hook/读取类命令仍走每核并发路径。

### 3.3 参考依据

- Intel SDM 与 MS 文档均把「处理器死锁」列为 0x101 首因（见 1.1）。
- HyperPlatform 在卸载 hook 时**不等待其它处理器**：它按每处理器数据独立回收，池耗尽直接 bugcheck 而不阻塞（见 2.3）。
- 看雪《基于VT EPT的无痕断点无痕hook原理及其应用》(2026-07) 描述的 MTF 流程是「放行一条指令 → MTF VM-exit 重新收紧权限」，**全程无跨核等待**。

### 3.4 修复方向（不引入新锁）

1. **删除 `wait_for_page_quiescence` / `wait_for_function_quiescence` 的跨核等待**，改为**引用隔离 + 延迟回收**：
   - 删除时只置 `removing=1` 并从链表摘除（新引用立即被拒），**不释放内存**；
   - 把该页记录挂入**本核的待回收链表**，在**下一个本核 MTF/EPT-exit 且引用计数已为 0** 时回收；
   - 引用计数已经存在（`hot_reference_count` / `mtf_reference_count`），回收条件用一次 `InterlockedCompareExchange(...,0,0)==0` 判定即可，**不阻塞**。
2. **引用计数归零的兜底**：在 `vmexit_monitor_trap_flag_handler` 与 `vmexit_ept_violation_handler` 的**所有返回路径**统一走一个 `release` 出口（当前 `VmExitEpt.cpp` 已有 `handled_return` / `unhandled_after_release` 两个出口，保持并确保 `EptWatchEvents`/`EptWatchInt3Execute` 内不再有提前 return 造成漏减）。
3. 若必须保留「等待」，则**降级为有界让步**：等待前先释放事务锁，`KeStallExecutionProcessor` 有限次数后按「引用未清 → 本核放弃删除并回报失败」，绝不在持锁状态无限自旋。但这只是兜底，首选仍是 1。

---

## 4 R3（P0）全局锁被跨核长操作持有

### 4.1 `vmcs_dump_lock` 内做 ~150 次 DbgPrint

`HypervisorRoutines/HypervisorVmcsDump.cpp:12` 取 `vmcs_dump_lock`，`:213` 才释放；中间 `:17-209` 是约 **150 次** `LogDump(...)`（每次含 VMREAD + `DbgPrintEx`）。入口在 `VmCall/VmcallDispatch.cpp:377` 的 `VMCALL_DUMP_VMCS_STATE`。

带内核调试器（WinDbg/串口）时 `DbgPrintEx` 极慢，持锁期间其它核的同类 VMCALL 全部自旋。

### 4.2 事务锁覆盖整页 guest 读 + split + 池分配 + 逐核 commit

- `EptWatchActivation.cpp:29-113`：`while (left > 0)` 逐页；每页内 `Cr3TrackerAcquire` + 逐 CPU `ept_watch_activate_internal`（可能 `split_pml2`、`pool_manager::request_pool`）。
- `EptWatchBreakpointInstall.cpp:49-151`：每页 `read_guest_virtual_memory`（整页 GVA→GPA 遍历 + `memcpy_safe`）+ `hide_cc_breakpoint` + 逐 CPU `commit_hide_cc_breakpoint`。
- 注释本身也承认「EPT split、池操作和跨 VCPU commit 均在表锁外执行」，**但仍在事务锁内**（`EptWatchActivation.cpp:97` 的注释与 `:134` 的 `unlock(&eptWatchTransaction_lock)` 之间）。

### 4.3 修复方向

- `dump_vmcs()`：**去掉锁**，改为「只采集到本核缓冲区，锁外/延后输出」；或限制为单次调用且由调用方保证不并发。日志本身是诊断用途，用一把跨核锁保护它属于典型的「为调试功能付出生产稳定性代价」。
- 事务锁内的重操作：按 R1 的「每核定向提交」拆解，使锁内只剩指针替换（`swap_pml1_and_invalidate_tlb` 的条目写入）。

---

## 5 R4（P0）自旋锁实现本身无任何保护

`Sync/spinlock.cpp:28-51`：

```cpp
void lock(volatile long* lock_)
{
    unsigned __int32 wait = 1;
    while (!try_lock(lock_))
    {
        for (unsigned __int32 i = 0; i < wait; ++i) _mm_pause();
        if (wait * 2 > max_wait) wait = max_wait; else wait = wait * 2;
    }
}
```

缺失项（逐条对照 Windows 内核自旋锁规范与 `KeAcquireSpinLock` 语义）：
- 不提 IRQL（`KeAcquireSpinLock` 会提到 DISPATCH_LEVEL；此处不提，因此在 PASSIVE 语境持锁时可能被 APC/线程调度影响）；
- 无 owner 记录、无递归检测、无超时、无死锁诊断；
- `unlock` 是裸写 `*lock_ = 0`，**无释放屏障**（`KeReleaseSpinLock` 隐含 `KeMemoryBarrier`）。在 x86-TSO 下基本够用，但配合上面的 `_mm_mfence()` 使用不一致，容易掩盖真实的内存序需求。

### 5.1 修复方向

**首选：减少到接近零。** 按 R1/R2/R3 拆解后，跨核共享可变状态应只剩：
- `eptWatchList[]`（256 项数组，全局）
- `g_Trackers[]`（Cr3Tracker）
- `g_vmm_context` 内的少量标量

对这些，建议改为**每核副本 + 单向发布**，或直接用 `Interlocked*` + 显式 `_mm_mfence()` 做无锁读写（本仓库已有 `_mm_mfence()` 使用习惯）。

**次选（若保留锁）：** 补 `KeMemoryBarrier()` 到 `unlock`，并把锁改名为「本核自旋锁」以避免误用为跨核锁。

---

## 6 R5（P1）根态文件 IO

### 6.1 事实

`Driver/Driver.h:36`：`#define outFile(...) VtLoadWriteLog(__VA_ARGS__)`
`Driver/VtLoadLog.cpp:189-238`：

```cpp
if (KeGetCurrentIrql() != PASSIVE_LEVEL)
{
    return STATUS_UNSUCCESSFUL;   // 唯一门闸
}
...
ZwCreateFile(... FILE_SYNCHRONOUS_IO_NONALERT ...);
ZwWriteFile(...);
ZwClose(file_handle);
```

问题：**VM-exit 不改变 IRQL**（见 1.4）。Dbgk 驱动从普通线程发起 `VMCALL` 时 IRQL = PASSIVE_LEVEL，根态处理该 VMCALL 时 IRQL 仍是 PASSIVE_LEVEL → 门闸**放行** → 在 **VMX 私有宿主栈**上执行 `ZwCreateFile/ZwWriteFile`。

后果（两条，任一都会导致 0x101）：
1. 同步文件 IO 可能等待 IO 完成 / 触发分页 → 处理器在根态阻塞（此时它既不在来宾也不在可被调度的宿主线程栈上）；
2. 私有宿主栈做文件 IO 会触发 `FAST_FAIL_INCORRECT_STACK`（`docs/04` 的 B16 已记录过同一风险）。

补充：`sLog`（`Common/Ring0/Comms/SerialPort.h:28-32`）被 `_SERIAL_DEBUG_LOG` 关闭，展开为空宏，因此串口路径目前**无风险**。但 `outFile` 在 `EptPageTables.cpp`（构建/清理）与 `DriverLifecycle.cpp` 等处活跃。

### 6.2 修复方向

把门闸从「IRQL」改为「**是否在 VMX 根态/私有栈上**」：在 `vcpu` 上维护一个 `in_root_context` 标志（VM-exit 入口置位、VMRESUME 前清零），`VtLoadWriteLog` 见该标志直接返回。IRQL 判据本身保留作为第二道。

---

## 7 R6（P1）exit 风暴 / EPT 权限未恢复

- `VmExit/VmExitDispatcher.cpp:306-312`：未收录的 exit reason 一律注入 `#GP`。若来宾反复触发同一未处理 exit（例如某个 MSR/指令被拦截但未实现），会形成 **VM-exit 风暴**，来宾无法推进 → 取不到时钟中断。
- `VmExit/VmExitEpt.cpp` 的 watch 权限恢复依赖 MTF；`Ept/Watch/EptWatchEvents.cpp:154` 的注释也点明「普通 EPT watch 会关闭整页权限；同页非目标地址也必须放行一条指令，避免反复 VM-exit 卡死」——说明作者已知此处存在 livelock 风险。当前恢复逻辑依赖 `RestoreEptPageProperties`（`EptWatchQuery.cpp:46-54`）里的 `hv::set_mtf(true)` 与 `VmExitEpt` 里的 `mark_pending_mtf_page` 配对，两处**不在同一临界区**，任何一处提前返回都可能造成权限不恢复。

### 修复方向
- default 分支增加「同 reason 连续计数」并在超限时**记录后放行**（而不是无限 #GP）；
- 把「置 MTF」与「登记 pending 页」合并为**同一个函数**（一次调用内完成），消除两处配对失败的可能。这与看雪文中「放行一条指令 → MTF 收紧」的原子配对一致。

---

## 8 R7 —— 已证伪并撤回（保留记录，说明结论如何被修正）

**原判断**：`EptWatchActivation.cpp:98-109` / `EptWatchBreakpointInstall.cpp:137-147` 在「当前核」改写其它核的 EPT，随后只做本核 `invept_all_contexts_func()`，因此其它核会继续用旧映射。

**证伪依据**（多路交叉核对后）：
1. `EptWatchActivation` / `EptWatchBreakpointInstall` 对应的命令（`VMCALL_WATCH_*`、`VMCALL_HIDE_SOFTWARE_BREAKPOINT`、`VMCALL_SET_SOFTWARE_BREAKPOINT_BUFFER`、`VMCALL_WATCH_DELETE`、`VMCALL_CLEAR_BREAKPOINT_STATE`）都在 `Common/Ring0/Hvm/VmcallDispatchPolicy.h:9-15` 的 `IsEptMutation()` 白名单内。
2. 白名单命令走 `broadcast_vmcall`（`DbgkSysWin11/Hvm/HypervisorGatewayBroadcast.cpp:233-280`）的「事务属主」路径：由 `KeSignalCallDpcSynchronize` 选举**单核**属主执行改表，随后 `KeSignalCallDpcSynchronize` 屏障，**屏障之后每个 CPU 都各自执行一次 `SafeVmCall(VMCALL_INVEPT_CONTEXT, true, 0, 0)`**（同文件 `:271-277`）。
3. 即：**改表核 → 全核屏障 → 逐核各自 INVEPT**。`INVEPT` 只作用于执行它的处理器（Intel SDM），而这里每个处理器都执行了一次，**跨核缓存实际被正确刷新**。`EptWatchActivation.cpp:133` 的本核 INVEPT 属冗余，不是缺陷。

**残留的真实风险（降级为 P2）**：
- 若有旁路绕过网关直接 `__vm_call`（例如 `Hook/vmx/vmx.cpp:59` 一类直接发 VMCALL 的路径），就会退化成「改他核 + 只本核刷新」；
- 当 `transaction_status != 1`（属主失败）时**全核都不刷新**，若回滚不彻底会残留中间权限。

**方法论教训**：上一版只看了 `EptWatchActivation.cpp` 局部就下了「跨核 INVEPT 缺失」的结论，没有追到 Dbgk 侧的广播刷新点。**局部视角会得出错误结论**，本报告已据此重写第 9 节的多维度核对方式。

---

## 9 main 历史提交审阅（针对 0x101 的演进）

| 提交 | 与 0x101 的关系 |
| --- | --- |
| `cb1197e` 修复多核 VT 进入与长时间调试冻结 | 已识别「长时间冻结」，并「将 Dbgk 全核 EPT 操作串行为单次事务，消除 N² VMCALL」。**但只覆盖 `IsEptMutation` 白名单**，Hook/读取类命令仍每核并发抢事务锁 → R2 死锁面仍在。同时「删除 MTF 对 Guest STI interruptibility 的伪造与恢复」是正确的（MTF 不是 Guest TF，见看雪文）。 |
| `ffd837b` 闭合 watch 删除与 MTF 生命周期 | **引入**了 `mtf_reference_count` 与 `wait_for_page_quiescence` 的跨核等待 —— 即 R2 的来源。设计意图（删除前阻止新引用并等待现有引用对称释放）正确，但**实现选择了无上限自旋 + 持全局锁**，在根态是不可接受的。 |
| `5700be5` / `81660d0` 将 watch 重操作移出表锁 | 方向正确（缩小 `eptWatchList_lock` 持有范围），但**事务锁仍覆盖全部重操作** → R3 未解决。 |
| `f3a2a40` 事务化隐藏 INT3 多页安装 | 同上；多页循环整体在事务锁内。 |
| `20ee6bb` 修复 NMI 恢复、LCP 解码、断点事务竞态 | 引入 `ExecuteReadSoftwareBreakpoint` 的事务锁（`VmcallDispatch.cpp:307`），该命令**不在** `IsEptMutation` 白名单 → 每核并发抢锁，扩大 R2 暴露面。 |
| `76b7072` VMM 核心稳定性 | 引入 `g_vmx_init_lock`（`Vmm/VmmProcessor.cpp:104`）—— 这是**初始化期**锁，非热路径，风险可接受。 |
| `22cf974` VMX 致命故障恢复 | 引入 `vmx_fatal_error` fail-fast（`VmExit/VmExitCommon.cpp:142-180`）。属有意设计，但与 0x101 无关（0x101 不是它触发的）。 |
| `4ac518a` / `a7694e5` / `859853f` | EPT 覆盖范围与 Host 状态，与 0x101 无直接关系（4ac518a 解决的是 Arg1=7 的 0x00020001）。 |

**结论**：0x101 相关的两处关键演进是 `ffd837b`（引入跨核忙等）与 `cb1197e`（只部分收敛广播事务），叠加 `20ee6bb` 扩大了并发抢锁面。

---

## 10 修复优先级与落地顺序（不新增锁、不加无意义边界判断）

| 步骤 | 动作 | 涉及文件 | 风险 |
| --- | --- | --- | --- |
| 1 | `wait_for_page_quiescence` / `wait_for_function_quiescence` 改为「不阻塞回收」：置 `removing` + 摘链 + 挂本核待回收链，引用归零后由本核下次退出回收 | `Ept/EptCommon.cpp`、`Ept/Watch/EptWatchDeactivate.cpp`、`Ept/Hook/Cleanup/EptHookCleanup.cpp`、`Ept/EptWatch.h` | 中（需保证不漏减引用） |
| 2 | `dump_vmcs()` 去锁，改本核缓冲 + 延后输出 | `HypervisorRoutines/HypervisorVmcsDump.cpp` | 低 |
| 3 | `outFile` 门闸改为「根态标志」而非 IRQL | `Driver/VtLoadLog.cpp`、`Driver/Driver.h`、`VmExit/VmExitDispatcher.cpp`（置/清标志） | 低 |
| 4 | 把 `VMCALL_EPT_RIP_HOOK` / `VMCALL_READ_SOFTWARE_BREAKPOINT` / `VMCALL_READ_EPT_FAKE_PAGE_MEMORY` 纳入「单事务」路径，或改为逐核定向提交 | `Common/Ring0/Hvm/VmcallDispatchPolicy.h`、`DbgkSysWin10/11/Hvm/HypervisorGatewayBroadcast.cpp` | 中 |
| 5 | 「置 MTF」与「登记 pending 页」合并为单函数 | `Ept/Watch/EptWatchEvents.cpp`、`Ept/Watch/EptWatchQuery.cpp`、`VmExit/VmExitEpt.cpp` | 中 |
| 6 | 跨核改 EPT 改为「每核定向提交 + 本核 INVEPT」 | `Ept/Watch/*`、`DbgkSysWin10/11/Hvm/*` | 高（结构性） |
| 7 | `spinlock::unlock` 补 `KeMemoryBarrier()` | `Sync/spinlock.cpp` | 低 |

> 步骤 1 与 2 是**收益最大、风险最低**的两项：直接消除「根态无限自旋」与「锁内慢 IO」两类 0x101 直接成因。

---

## 11 参考依据

1. Microsoft Learn《Bug 检查 0x101 CLOCK_WATCHDOG_TIMEOUT》—— 0x101 定义、四个参数、成因（处理器无响应/死锁）。
2. Intel SDM Vol.3C 27.5.2「Loading Host State」—— VM-exit 装载 host RFLAGS（IF=0）。代码侧印证：`VT_Driver/Vmcs/VmcsHost.cpp` 无 HOST_RFLAGS 字段。
3. `tandasat/HyperPlatform`（本仓库 spinlock 注释声明派生自 Hvpp）—— EPT 数据 `// EPT related data stored in ProcessorData`、VM-exit 路径无自旋锁、仅 `InterlockedIncrement`、池耗尽直接 bugcheck、`HYPERPLATFORM_PERFORMANCE_MEASURE_THIS_SCOPE()`。
4. 看雪《基于 VT EPT 的无痕断点无痕 hook 原理及其应用》(2026-07-13) —— MTF 单步「放行一条指令 → MTF VM-exit 重新收紧权限」；MTF 不等于 Guest TF、不占 DRx。
5. Windows x64 IRQL 序：CLOCK_LEVEL(13) < IPI_LEVEL(14)；`KeIpiGenericCall` / `KeGenericCallDpc` 在被调用处理器上以 IPI_LEVEL 运行例程。
6. 本仓库既有文档：`docs/01-VT_Driver-逻辑梳理.md`（每核 EPT 不变量）、`docs/04-Bug清单与修复记录.md`（B16 私有栈文件 IO 风险）。

---

## 12 待确认

1. R2 的死锁构造需要「持有 pending MTF 的核随后进入抢事务锁的 VMCALL」。是否在实机抓到过 `0x101` 转储？若有，Arg3（PRCB）/Arg4（处理器索引）可确认是**单核**挂起还是**多核同时**挂起——多核同时挂起即指向 R1/R2 的全局锁，单核挂起则更可能是 R3/R5。
2. R5 是否在实机日志中见过 `FAST_FAIL_INCORRECT_STACK`？若有，则 R5 优先级应提到 P0。
3. 步骤 6（每核定向提交）属结构性重构，需要作者确认是否接受「控制线程逐核绑核」带来的时延（每核一次上下文切换，24 核约几十微秒级，远低于 0x101 阈值）。
