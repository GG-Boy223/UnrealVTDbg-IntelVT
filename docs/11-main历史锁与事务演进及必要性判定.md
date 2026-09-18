# 11 main 历史锁与事务演进 · 必要性判定

本文回答三个问题：
1. main 主线**全部 26 个节点**里，新增了哪些锁 / 事务 / 同步机制？
2. 每一项**是否真的需要**？
3. 与 0x101 的关系是什么？

范围：全量 Intel-VT 相关源码（见 §1），不限于单一文件。

---

## 1 全量 Intel-VT 源码面（先定位，再分析）

| 区域 | 文件数 | 行数 | 职责 |
| --- | --- | --- | --- |
| `VT_Driver/` | 187 | 21418 | VMX root 态核心：VMXON/VMCS、EPT、VM-exit、Host 页表、CR3 |
| `DbgkSysWin10/Hvm/` | 9 | 1782 | Win10 侧网关 + 跨核广播 + VMCALL |
| `DbgkSysWin11/Hvm/` | 9 | 1781 | Win11 侧同上（独立维护） |
| `Common/Ring0/Hvm/` | 12 | 748 | 共享策略：`VmcallDispatchPolicy.h`、`EptCoveragePolicy.h`、`VmxCapabilityPolicy.h`、`ntapi.h`（`KeGenericCallDpc` 声明） |
| `Common/Ring0/Spinlock/` | 2 | 69 | 自旋锁实现 |
| `Hook/vmx/` | 8 | — | 用户态侧 Vmx 广播 / 亲和性 / 断点状态 |
| `VT_Driver/ASM/` | 6 | — | `vm-exit.asm`、`vm-launch.asm`、`vm_context.asm`、`AsmCallset.asm` 等 |

**同步机制只集中在 4 个文件里**：`VT_Driver/Sync/spinlock.cpp`（原语）、`VT_Driver/Globals/Globals.cpp`（锁变量定义）、`DbgkSysWin10/11/Hvm/HypervisorGatewayBroadcast.cpp`（广播事务）、`Common/Ring0/Hvm/ntapi.h`（IPI API）。**其余 200+ 文件的同步全部是对这 4 处的调用**——所以本文的判定可以覆盖全部。

---

## 2 同步机制清单与引入节点（`git log -S` 逐个追溯）

| # | 机制 | 定义处 | **引入节点** | 引入时的理由 |
| --- | --- | --- | --- | --- |
| S1 | `vmcs_dump_lock` | `Globals.cpp` | **a9ef865**（最早） | 防止多核 VMCS 转储输出交错 |
| S2 | `eptWatchList_lock` | `Globals.cpp:14` | **a9ef865** | 保护全局 256 项 `eptWatchList[]` |
| S3 | 池锁 `lock_for_reading_pool` / `lock_for_request_allocation` | `poolmanager` | **a9ef865** / **1e70867** | 保护全局池管理器 |
| S4 | `KeGenericCallDpc` 全核广播 | `Common/Ring0/Hvm/ntapi.h` | **a9ef865** | 让所有 CPU 同时执行 EPT 变更 / INVEPT |
| S5 | `g_TrackerLock` | `Cr3Tracker.cpp:10` | **ba4c0d1** | 保护 128 槽 Cr3Tracker |
| S6 | `eptWatchTransaction_lock` | `Globals.cpp:15` | **ffd837b** | 「安装/删除事务锁：重操作可在表锁外串行执行」 |
| S7 | `mtf_reference_count` + 忙等 `while(...) _mm_pause();` | `MtfPendingState.cpp` / `EptWatchDeactivate.cpp` | **ffd837b** | 「删除前阻止新引用并等待现有引用对称释放」 |
| S8 | `hot_reference_count` + `wait_for_page_quiescence` / `wait_for_function_quiescence` | `EptCommon.cpp:301-329` | **9ddfe75**（原语出自 ffd837b） | 「热引用排空机制」「修复 MTF pending 期间卸载导致的 UAF」 |
| S9 | `hooked_page_list_lock` | `EptCommon.cpp:83` | **9ddfe75** | 保护每核 `hooked_page_list` |
| S10 | `functions_lock` | `EptCommon.cpp:193` | **9ddfe75** | 保护每页函数链表 |
| S11 | `g_vmx_init_lock` | `Globals.cpp:18` | **76b7072** | 「P0-003：多核 VMX 初始化自旋锁，防止并发 VMXON 竞态条件」 |
| S12 | 广播侧事务模式（`KeSignalCallDpcSynchronize` 选举属主） | `HypervisorGatewayBroadcast.cpp` | **cb1197e** | 「将 Dbgk 全核 EPT 操作串行为单次事务，消除 N² VMCALL」 |

### 2.1 一个必须指出的历史事实

**26 个节点中，7 个节点新增了锁/同步机制（a9ef865 → ba4c0d1 → ffd837b → 81660d0 → 9ddfe75 → cb1197e → 76b7072 → 1e70867），而 `git log -S` 显示没有任何一个节点删除过上述任何一把锁。**

其中 `81660d0`（「将普通 watch 重操作移出全局表锁」）与 `5700be5`（「将 watch 删除重操作移出表锁」）的改动方向尤其值得注意——看 `81660d0` 的实际 diff：

```diff
-		spinlock::lock(&eptWatchList_lock);
+		spinlock::lock(&eptWatchTransaction_lock);
...
-			//设置每个cpu
+			// EPT split、池操作和跨 VCPU commit 均在表锁外执行，VM-exit 热路径不会长时间自旋。
```

即：**把重操作从「表锁」里搬到了「新引入的事务锁」里**。问题的形态变了（表锁持有时间缩短），但**「一把全局锁跨越跨核重操作」这个结构原封不动**。提交信息里「VM-exit 热路径不会长时间自旋」只对 `eptWatchList_lock` 成立，对 `eptWatchTransaction_lock` 不成立。

> 这是一个清晰的模式：**每次都在「缩短一把锁的持有时间」时引入另一把锁，从未质疑「为什么需要一把全局锁」。**

---

## 3 逐项必要性判定（核心问题：本项目是否真的需要）

判定依据统一为：**该锁保护的状态是否真的跨核共享？如果不是，锁就是多余的。**

| # | 机制 | 保护的状态 | 该状态是否真的跨核共享 | **判定** |
| --- | --- | --- | --- | --- |
| S11 | `g_vmx_init_lock` | VMXON/VMCS 初始化序列 | **否** | **【完全不需要，应删除】** |
| S6 | `eptWatchTransaction_lock` | Hook/Watch 安装删除事务 | **否**（EPT 每核独立） | **【不需要，应删除】** |
| S4 | `KeGenericCallDpc` 全核广播 | 跨核同步执行 EPT 变更 | **否**（EPT 每核独立） | **【不需要，应删除】** |
| S1 | `vmcs_dump_lock` | VMCS dump 输出 | 是（但只是调试输出） | **【不需要，改本核缓冲】** |
| S2 | `eptWatchList_lock` | 全局 256 项 `eptWatchList[]` | **是，但这是设计选择的结果** | **【应降级为每核表】** |
| S9 | `hooked_page_list_lock` | 每核 `hooked_page_list` | **否**（是每核的） | **【应改为「谁拥有谁访问」】** |
| S10 | `functions_lock` | 每页函数链表 | **否**（是每页的） | **【同上】** |
| S7/S8 | `mtf_reference_count` / `hot_reference_count` + 忙等 | 页/函数记录的存活期 | 是（引用来自其它核） | **【机制需要，忙等不需要】** |
| S3 | 池锁 | 全局池管理器 | **是** | **【锁需要，但不得在根态调用】** |
| S5 | `g_TrackerLock` | 128 槽 Cr3Tracker | 是 | **【锁需要，但取决于 Cr3Tracker 是否保留】** |
| S12 | 广播侧事务模式 | 选举单核属主执行改表 | — | **【在 S4 被删除后可一并去掉】** |

### 3.1 判定 S11（`g_vmx_init_lock`）：证据最硬的一条

`76b7072` 的理由是「防止**并发** VMXON 竞态」。但：

- `initalize_vcpu` 全仓库**只有一个调用点**：`Vmm/VmmEntry.cpp:41`；
- 该调用点位于**顺序 `for` 循环**内，每轮以 `KeSetSystemGroupAffinityThread` 把**同一个线程**绑到目标核（`VmmEntry.cpp:38`），执行完再 `KeRevertToUserGroupAffinityThread`（`:59`）；
- 全仓库**没有任何** `KeGenericCallDpc` / `KeIpiGenericCall` 去调用初始化路径。

→ **不存在并发，锁保护的是不存在的竞态。这是本项目最明确的一把「无意义锁」。**

讽刺的是，同一个提交 `76b7072` 的说明里还写着「删除无意义的边界检查」——作者已有「删无意义检查」的自觉，但没把同一标准用到锁上。

### 3.2 判定 S6 + S4（事务锁 + 全核广播）：**这是 0x101 的根**

这两项是配套的，也是本项目**最大的结构性冗余**：

- 事实：`__ept_state` 挂在 `__vcpu` 上，每核各自建 EPT、各自读能力 MSR（`docs/04` B17）。**数据模型是 per-core 的，且是正确的。**
- 矛盾：同步模型却假设「EPT 是一份全局资源」——所以才有全局事务锁（S6）和「让所有核同时改」的广播（S4）。
- 后果：`Ept/Watch/EptWatchActivation.cpp:98-109` 与 `EptWatchBreakpointInstall.cpp:137-147` 在**当前核**上循环改写**所有核**的 EPT——这个动作本身只在「全局模型」下才需要。
- **0x101 由此而生**：24 个核在 `KeGenericCallDpc` 的 IPI_LEVEL 屏障下同时进根态，串行排队等 S6，而 S6 锁内是重操作。详见 `docs/10` 的 P1/P9。

**如果改成「每个核只改自己的 EPT」**：S6 消失（无需跨核事务）、S4 消失（无需广播）、P1 消失、P9 消失、`hooked_page_list_lock` 的跨核访问（S9）也自然消失。

### 3.3 判定 S7/S8（引用计数 + 忙等）：**机制要留，实现要换**

- **机制是解决真实问题的**：`9ddfe75` 的说明明确写「修复 MTF pending 期间卸载导致的 UAF 漏洞」。页/函数记录确实可能被其它核的热路径（EPT violation / MTF）持有，删除时必须等它们退出。**这部分必须保留。**
- **实现不可接受**：`EptCommon.cpp:301-329` 的 `while(...) _mm_pause();` 无上限、无让步，且被 `EptWatchDeactivate.cpp:74`、`EptHookCleanup.cpp:59/114/164/188` **在持有 S6 事务锁时**调用。
- **换法**：引用计数已存在，只需把「等」改成「不阻塞回收」——置 `removing` + 摘链（新引用立即被拒）+ 挂本核待回收链，在本核下次退出时用一次 `InterlockedCompareExchange(...,0,0)==0` 判定后回收。**安全性完全等价，但不再阻塞。**（对照 HyperPlatform：池耗尽直接 bugcheck，绝不阻塞。）

### 3.4 判定 S2/S9/S10（三把细粒度锁）：**方向反了**

- S9（`hooked_page_list_lock`）与 S10（`functions_lock`）保护的**本来就是每核/每页的私有数据**，本不需要锁。它们存在是因为**存在跨核访问**：`Ept/Watch/EptWatchDeactivate.cpp:160-191` 从当前核遍历**其它核**的 `hooked_page_list`，**而且这个遍历没有持 S9**——即锁的存在与使用方式互相矛盾（持锁的路径是安全的，不持锁的路径反而在跑）。
- 正确方向是「**谁拥有谁访问**」：删除事务改成逐核定向执行（每核删自己表里的记录），S9/S10 的跨核必要性消失，只剩「本核内 VM-exit 与 VMCALL 两条路径互斥」这一极短临界区。

### 3.5 判定 S5（`g_TrackerLock`）：**先决定功能是否保留**

`docs/09` 已查明：`Cr3TrackerObserve` 全工程只剩 **1 个调用点**（`VmExit/VmExitEpt.cpp:120`），CR3-load exit 路径（`VmExit/VmExitControl.cpp:38-39`）只 Publish/Remember 不 Observe，`Cr3TrackerWorker.cpp` 的 `TryConsumeCandidate` **无任何调用者**。即 Cr3Tracker 的「观察/更新」侧已实际不生效，只有 `Acquire`/`Release` 还在被 watch 安装路径使用。

→ **先决定这个功能是否保留**。若保留：锁需要，但应把 Observe 侧接通或明确移除；若移除：S5 与整个 Cr3Tracker 一并删除。

---

## 4 结论：本项目真正需要多少锁

**真正需要的只有 2 类：**

| 类别 | 原因 | 约束 |
| --- | --- | --- |
| **池管理器锁**（S3） | 池是真正的全局共享资源 | **不得在 VMX root / VM-exit 上下文中调用**（改为耗尽即失败） |
| **引用计数**（S7/S8 的机制部分） | 跨核引用隔离，防 UAF，解决真实问题 | **不得配合忙等**；改为不阻塞回收 |

**其余全部可删除或降级：**

| 机制 | 处置 | 风险 |
| --- | --- | --- |
| S11 `g_vmx_init_lock` | **直接删除**（不存在并发） | 极低 |
| S1 `vmcs_dump_lock` | **删除**，改本核缓冲 | 极低 |
| S6 `eptWatchTransaction_lock` | **删除**（改逐核定向提交后无必要） | 中 |
| S4 `KeGenericCallDpc` 广播 | **删除**（同上） | 中 |
| S12 广播事务模式 | 随 S4 一并删除 | 中 |
| S2 `eptWatchList_lock` | 降级为**每核 watch 表** | 中 |
| S9 `hooked_page_list_lock` / S10 `functions_lock` | 改「谁拥有谁访问」后仅剩本核极短临界区 | 中 |
| S5 `g_TrackerLock` | 取决于 Cr3Tracker 是否保留 | 低 |

**一句话结论**：本项目**并不需要**现在这套跨核锁与广播体系。它的 EPT 数据模型是每核独立的（这是对的、也是它的加分项），但同步模型却按「全局共享资源」来设计——**S6 + S4 是这一错配的产物，也正是 0x101 的直接来源**。把跨核操作改成逐核定向提交后，**11 项同步机制可以收敛到 2 项**，且 0x101 的 P1/P8/P9 三条主通路一并消失。

---

## 5 与 0x101 的对应关系（只列相关项）

| 同步机制 | 对应 0x101 通路（`docs/10`） | 删除后的效果 |
| --- | --- | --- |
| S6 事务锁 | **P1**（锁内重活）、**P8**（与忙等互相等待） | 两条通路消失 |
| S4 广播 + S12 事务模式 | **P9**（IPI_LEVEL 双重屏蔽）、**P12**（卸载路径跨核等待） | 两条通路消失 |
| S7/S8 忙等 | **P7、P8** | 改为不阻塞回收后消失 |
| S1 `vmcs_dump_lock` | **P2**（150 次 DbgPrint 持锁） | 消失 |
| S3 池锁（根态调用） | **P4**（池耗尽根态分配） | 改为失败返回后消失 |

**P3（根态文件 IO）、P5（持锁整页读）、P10（交替 GPA 零前进）、P11（#GP 风暴）与锁体系无关**，需单独按 `docs/10` §5 处理。

---

## 6 待确认

1. **`Cr3Tracker` 是否保留？** 决定 S5 的处置，也决定 `Cr3TrackerAcquire/Release` 是否还出现在 watch 安装路径。
2. **`hooked_page_list` 的跨核遍历是否有意为之？** `EptWatchDeactivate.cpp:160-191` 从当前核遍历其它核的表且不持锁——若这是设计，则「每核独立」的前提在删除路径上并未贯彻。
3. **`mtf_reference_count` 是否存在未配对的递增？** 若有，`docs/10` 的 P7/P8 从「需要条件」变为「必然」。建议逐点核对 `mark_pending_mtf_page` 的每次成功返回是否都有对应 `clear_pending_mtf_metadata`。
4. **`S4` 删除后如何刷新其它核的 EPT？** 逐核定向提交的语义是「本核改本核 + 本核 INVEPT」，天然自洽；但若保留「一核改全核」，则必须保留逐核 INVEPT（见 `docs/08` §8 的修正）。这两条路线必须二选一，不能混。
