# 13 GitHub Intel-VT 项目全景对照

本文扫描 GitHub 上 Intel VT-x / VMX 相关开源项目（20+ 个）与生产级 hypervisor，按**十个技术维度**与本项目逐项对照，并回答「本项目在开源界处于什么位置、该采纳什么、该拒绝什么」。

前置：`docs/12-最终结论.md`（0x101 结论）、`docs/11`（锁必要性判定）、`docs/10`（0x101 全通路）。

---

## 1 项目清单

### 1.1 经典研究型

| 项目 | 语言 | 特点 | 与本项目关系 |
| --- | --- | --- | --- |
| `tandasat/HyperPlatform` | C++ | Type-1，EPT 基础设施（**不含 EPT hook**） | 本项目 spinlock 注释声明派生自 hvpp；HyperPlatform 是 EPT 构建的参考 |
| `tandasat/DdiMon` | C++ | **EPT hook + MTF 的参考实现**（`shadow_hook.cpp`） | MTF 标准范式来源 |
| `tandasat/MiniVisorPkg` | C | UEFI Type-1 | **唯一把「IF=0 ⇒ IRQL≥HIGH_LEVEL ⇒ 禁 API/禁 IPI」写进代码注释的项目** |
| `wbenny/hvpp` | C++ | Type-2，工程化好 | 本项目 spinlock 的直接来源 |
| `ionescu007/SimpleVisor` | C | 教学 | EPT 完全 per-VP |

### 1.2 现代/活跃

| 项目 | 语言 | 特点 | 与本项目关系 |
| --- | --- | --- | --- |
| **`HyperDbg/hyperdbg`** | C | **VT 调试器**，与本项目定位最像 | 广播分层、延迟释放池、MTF 三态幂等 |
| `SinaKarvandi/Hypervisor-From-Scratch` | C | 教学（2.7k star） | EPT hook 全流程 |
| `hititou/VMXHypervisorToolbox` | C | 含 `intel-vmx-code-audit.md` 审计文档 | 审计发现可直接对照 |
| `DarthTon/HyperBone` | C++ | 有跨核屏障 | `KeSignalCallDpc` 屏障范式 |
| `stonedreamforest/Mirage` | C | 反反调试 | 仅 README，无源码 |
| `WWFWT/TinyVT` | C++ | EPT 无痕 hook | **反面教材**（全局单份 EPT、无锁、无 INVEPT） |

### 1.3 Rust / 其他

| 项目 | 语言 | 特点 |
| --- | --- | --- |
| `memN0ps/illusion-rs` | Rust | UEFI Type-1；**反面教材**（VM-exit 内持全局 `spin::Mutex` + 分配内存） |
| `memN0ps/matrix-rs` | Rust | Type-2，共享 EPT |
| `Zero-Tang/NoirVisor` | C/Rust | 嵌套虚拟化；**per-vCPU EPT 范式** |
| **`jonomango/hv`** | C | 轻量；**固定数组 free-list + 只摘链不释放**（最优雅的免 UAF 设计） |
| `amiryeshurun/HyperWin` | C | 计数门闩替代屏障 |
| `KelvinMsft/kHypervisor` | C | 预分配池 + 引用计数 |
| `Bareflank/hypervisor` | C++ | 工程化 Type-1 |
| `chillancezen/ZeldaOS.x86_64` | C | 裸机；全局单份 EPT |

### 1.4 生产级

`KVM`（`arch/x86/kvm/vmx/`）、`Xen`（`xen/arch/x86/hmm/p2m-ept.c`）、`ACRN`（`hypervisor/arch/x86/guest/ept.c`）、`Microsoft Hyper-V`（仅公开文档）。
> `IntelLabs/hypervisor-for-security` 的两个 GitHub 路径均 404，无法溯源，不纳入。

---

## 2 十维度横向对照

### A. EPT 数据组织

| 项目 | 组织方式 |
| --- | --- |
| HyperPlatform | **per-processor**（`EptData` 注释 `// EPT related data stored in ProcessorData`） |
| hvpp | **per-vCPU**（`vcpu_t::ept_`） |
| DdiMon | 两层：`ShadowHookData`（per-processor）+ `SharedShadowHookData`（全局，`shared_ptr` 引用计数） |
| MiniVisorPkg / SimpleVisor / HyperBone / VMXHypervisorToolbox | **per-VP / per-vCPU** |
| HyperDbg / Hypervisor-From-Scratch | **per-processor**（`g_GuestState[i].EptPageTable`） |
| NoirVisor / jonomango-hv / HyperWin / kHypervisor | **per-vCPU / per-CPU** |
| TinyVT / ZeldaOS / illusion-rs / matrix-rs | 全局单份 |
| **本项目** | **per-vCPU（`__ept_state` 挂在 `__vcpu` 上）——正确** |

> **关键观察**：所有 per-vCPU/per-processor 派（HyperPlatform、hvpp、MiniVisor、SimpleVisor、HyperBone、HyperDbg、HFS、NoirVisor、hv、HyperWin、kHypervisor）**全部不需要跨核 EPT 锁**。共享派（TinyVT、ZeldaOS、illusion-rs、matrix-rs）才有跨核问题。
>
> **本项目属于 per-vCPU 派，却按共享派的方式做同步——这是全场唯一的错配。**

### B. VM-exit 路径约束

| 项目 | 做法 |
| --- | --- |
| **MiniVisorPkg** | 注释原文：`"executed while interrupt is disabled via RFLAGS.IF being 0"`、`"essentially equal to IRQL being HIGH_LEVEL (i.e., at a higher IRQL than IPI_LEVEL)"`、`"unsafe to call any Windows provided API that is not stated as callable at HIGH_LEVEL"`、`"IPI, if requested, is never delivered and causes deadlock"` |
| hvpp | 注释：`"called with IRQL at IPI_LEVEL ... effectively forbids us to call most of the kernel functions"`、`"At that level, we cannot call ExAllocatePoolWithTag"` |
| HyperBone | 文件头 `IRQL HIGH_LEVEL`，代码 `KeRaiseIrql(HIGH_LEVEL, &GuestIrql)` |
| HyperPlatform | 热路径**不分配池内存**；池耗尽 `HYPERPLATFORM_COMMON_BUG_CHECK`；日志用 `_SAFE` 变体（`"neither calls DbgPrint() nor writes to a file"`） |
| kHypervisor | 预分配 50 项，超限 bugcheck |
| jonomango-hv | 固定 `free_page_pfns[]` 数组，耗尽静默 `return` |
| KVM | `vcpu_enter_guest()` 顺序 `preempt_disable()` → `local_irq_disable()` → `mode = IN_GUEST_MODE`；退出后立即恢复，**重活不在 root 内做** |
| **本项目** | 有预分配池，但**耗尽时同步 `ExAllocatePool`**（`poolmanager.h:130-136`）；**在根态做文件 IO**（`VtLoadLog.cpp:189-238`） |

> **本项目是唯一在 VM-exit 路径做文件 IO 的项目。** 而 HyperPlatform 明确用 `_SAFE` 日志变体正是为了避开这一点。

### C. Hook 安装/卸载与 UAF

| 项目 | 机制 | 如何避免 UAF |
| --- | --- | --- |
| DdiMon | EPT 权限 + 双影子页 | **无引用计数**，靠 `UtilSleep(1000)` 粗糙延时（README 自认 "not robust"） |
| **HyperDbg** | execute-only 页 + 影子页 | **延迟释放池**：`"we add the hooked entry to the list of pools that will be deallocated on next IOCTL"` |
| **jonomango/hv** | EPT hook | **固定数组 free-list，节点只摘链不释放** → **结构性免 UAF**，无需任何等待 |
| HyperBone | EPT hook | `KeGenericCallDpc` 屏障（卸载后即释放，靠屏障保证安全）；自留 TODO `// TODO: atomic memory patching` |
| Hypervisor-From-Scratch | execute-only + 影子页 | 无跨核等待，仅 `HvNotifyAllToInvalidateEpt()` |
| TinyVT | host 改 PTE | **卸载后立即 `kfree`，无 INVEPT → 风险最大** |
| VMXHypervisorToolbox | per-CPU PTE 隔离 | 用 per-CPU 私有 PTE **替代等待** |
| **本项目** | EPT 权限 + 影子页 + 跳板 | **引用计数 + 无上限跨核忙等**（最重的做法） |

> **本项目的做法在开源界没有对应物**：引用计数本身合理，但「持全局锁 + 无上限忙等」是全场唯一。
> `jonomango/hv` 的「固定数组 + 只摘链不释放」是**更简单且更安全**的替代方案。

### D. MTF

| 项目 | 做法 |
| --- | --- |
| **DdiMon** | 三行**同一函数内紧邻**：`ShpEnablePageShadowingForRW()` → `ShpSetMonitorTrapFlag(true)` → `ShpSaveLastHookInfo()`；`ShpSaveLastHookInfo` 有 `NT_ASSERT(!sh_data->last_hook_info)`，`ShpRestoreLastHookInfo` 有 `NT_ASSERT(...)` 且随后置 `nullptr` → **幂等** |
| **HyperDbg** | 三态幂等（`IgnoreMtfUnset` / `IgnoreOneMtf` / `MtfEptHookRestorePoint`），未命中则 `LogError("Err, why MTF occurred?!")`；`Ept.c` 有长注释解释「执行耗时长会导致 MTF 落在别的例程」 |
| Hypervisor-From-Scratch | 同样紧邻，但**无防重入标记** |
| HyperBone / NoirVisor / jonomango-hv | 都有「记录待恢复页 + 置 MTF + 恢复」的配对，各有幂等设计 |
| TinyVT | `ExitMonitorTrapFlag` **未实现**（只 Log） |
| **本项目** | **拆在两个不同临界区**：`RestoreEptPageProperties()`（`EptWatchQuery.cpp:53`）里 `set_mtf(true)`，`VmExitEpt.cpp` 里 `mark_pending_mtf_page()` → **配对可失败**（对应 0x101 通路 P10） |

### E. 跨核 EPT 刷新

| 项目 | 做法 |
| --- | --- |
| HyperPlatform / DdiMon / NoirVisor / jonomango-hv / kHypervisor / illusion-rs | **全部只做本地 INVEPT**（因为 EPT 每核独立） |
| **HyperDbg** | 共享 EPT → `BroadcastNotifyAllToInvalidateEptAllCores()` = `KeGenericCallDpc(...)`；**但 DPC 回调里只发一次 `AsmVmxVmcall(VMCALL_INVEPT_*)` + 一次同步屏障，重活留给 root** |
| **KVM** | `kvm_flush_remote_tlbs()` → `KVM_REQ_TLB_FLUSH`，**延迟到该核下次进 guest** |
| **Xen** | `ept_sync_domain()` 置 `ept->invalidate` cpumask；`defer_flush` 时只置 `need_flush`；注释 `"Invalidation will be done in vmx_vmenter_helper()"` |
| **ACRN** | `ept_flush_guest()` 注释 `"Here doesn't do the real flush, just makes the request which will be handled before vcpu vmenter"` |
| **Hyper-V** | TLB "locked" 时**挂起调用者并重发** hypercall，用调度替代忙等 |
| **本项目** | 广播 + 逐核各自 INVEPT（**正确但最重**）；且 DPC 回调里做的是完整 VMCALL 重活 |

> 注意：`docs/08` §8 已撤回「本项目只做本核 INVEPT」的错误判断——当前广播路径是每个核都各自 INVEPT，**功能正确**。问题在**代价**：HyperDbg 的 DPC 回调只发一次 VMCALL，本项目在回调里做完整事务。

### F. 同步原语

| 项目 | 做法 |
| --- | --- |
| hvpp / Hypervisor-From-Scratch | `atomic_flag` + 指数退避（`max_wait=65536`），**不抬 IRQL** —— **前提是调用点已在 VM-exit** |
| jonomango-hv | RAII `scoped_spin_lock` + **极短临界区** |
| KVM | `mmu_lock`（rwlock）**锁内只改 spte**；不可睡眠上下文用 `raw_spinlock_t` + `lockdep_assert_irqs_disabled()` |
| Xen | `p2m_lock` 保护元数据；无锁读用 `atomic_read_ept_entry()` + `smp_wmb()` |
| ACRN | `spinlock_obtain(&vm->ept_lock)` **仅包住 pgtable 修改**，`ept_flush_guest()` 在锁外 |
| HyperDbg | `DpcRoutines.c` 注释：不能用 Windows 自旋锁，因其 `"raises the IRQL to DPC… If we're currently on the right core, we never find a chance to release the spinlock so a deadlock happens"` |
| **本项目** | 用 hvpp 同款自旋锁，但**用在了非 VM-exit 上下文**（VMCALL from DPC/IPI_LEVEL）→ **前提被破坏**；且锁内做重活 |

> **共识**：锁内只改元数据，flush/IPI 一律 request 化。本项目的 `eptWatchTransaction_lock` 是全场**唯一把「整页 guest 读 + split + 池分配 + 逐核 commit」放进锁内**的。

### G. VMX 指令 / VM-entry 失败

| 项目 | 策略 |
| --- | --- |
| HyperPlatform | 初始化期：记录日志 + `goto Exit` + `__vmx_off()` + 清理；运行期：fail-fast |
| hvpp / SimpleVisor | 记录 + 返回错误 + `stop()` 回滚，**不蓝屏** |
| **KVM** | `handle_ept_misconfig()` → `kvm_mmu_page_fault()` → **能修则修**，MMIO 交 `x86_emulate_instruction` |
| **Xen** | `ept_handle_misconfig()` → `resolve_misconfig()` **尝试修复** |
| ACRN | `ASSERT(status == 0, "EPT Misconfiguration is not handled.\n")` → fail-fast |
| **本项目** | **一律 `KeBugCheckEx`**（含 guest triple fault） |

> **本项目把 guest 故障升级为宿主蓝屏，在开源界少见。** HyperPlatform/hvpp/SimpleVisor 都区分「初始化期/guest 侧」与「宿主侧不可恢复」两类。

### H. CPU 代际差异

| 项目 | 做法 |
| --- | --- |
| 全场（HyperPlatform / SimpleVisor / hvpp / HyperDbg / TinyVT / VMXHypervisorToolbox / illusion-rs / kHypervisor） | **能力 MSR 驱动 + 局部回退，无型号硬编码**：`ctl &= msr.high; ctl |= msr.low`；用 `IA32_VMX_BASIC.vmx_capability_hint` 选 TRUE_* 系列 |
| KVM | `struct vmx_capability { u32 ept; u32 vpid; }` + `cpu_has_vmx_*()` 按位查询；`SECONDARY_EXEC_*` 同理 |
| 少数硬编码 | HyperBone `PageWalkLength=3`；ZeldaOS/NoirVisor 硬编码 memory_type |
| **本项目** | **符合**（`ajdust_controls` + `IA32_VMX_BASIC.true_controls`），且**每核独立建 EPT** 正确处理了大小核能力差异 —— **这是本项目相对多数开源实现的加分项** |

### I. 物理内存直映 / Host 页表

| 项目 | 做法 |
| --- | --- |
| **MiniVisorPkg** | **不自建 host 页表**：注释 `"On Windows, no special set up is done because the host shares the System process CR3"`，`g_HostCr3 = __readcr3()` |
| **hvpp** | **不自建**：`memory_mapper.cpp` 预留 1 页 VA + 改 PTE + `invlpg`，`pte_->global = true` |
| **HyperDbg** | **不自建**：`MmAllocateMappingAddress` 预留系统 VA + 改 PTE + `CpuInvlpg` |
| **HyperPlatform** | **不自建**：特征码扫描定位 `PTE_BASE` + `MmGetVirtualForPhysical` |
| **SimpleVisor** | 直接用 `SystemDirectoryTableBase`（加载回调捕获） |
| jonomango-hv | 自建 `prepare_host_page_tables()` 直映全部物理内存 |
| illusion-rs | 自建 `host_paging` + `build_identity()`，`IDTR_BASE = u64::MAX // Bogus` |
| **本项目** | **自建 host 页表 + 512GB 直映 + VM-exit 关键页动态补映射**（最重的做法） |

> **主流 Windows hypervisor 都不自建 host 页表**（复用 System process CR3 或用 1 页预留 VA 改 PTE）。本项目的自建方案引入了 0x101 通路 P3/P5 与 Host PCID 别名问题（见 `docs/09`）。

### J. 中断与事件注入

| 项目 | 异常位图 | `external_interrupt_exiting` | pending 冲突处理 |
| --- | --- | --- | --- |
| HyperPlatform | 0（示例位全注释） | — | 无显式处理 |
| MiniVisorPkg | 无 | **不开** | 无 |
| hvpp | — | — | **`interrupt_queue_t`（64 深度 FIFO，分 external/NMI 两条）** |
| **HyperDbg** | 经 `HvSetExceptionBitmap` 封装 | `HvSetExternalInterruptExiting` | **`PendingExternalInterrupts[]` + interrupt-window exiting（排队+窗口重投）** |
| jonomango-hv | 0 | 不开 | **`queued_nmis` + `nmi_window_exiting`（最完整的 NMI 排队）** |
| KVM | — | `PIN_BASED_EXT_INTR_MASK` | `inject_pending_event()` + `enable_irq_window()` + `KVM_REQ_EVENT` |
| **本项目** | 0 | **不开** | **`inject_interruption` 遇 pending 直接 `return`（静默丢弃，`HypervisorInterrupts.cpp:30-33`）** |

> **本项目是唯一「遇 pending 静默丢弃」的项目。** hvpp/HyperDbg/jonomango-hv 都有显式 pending 队列。
> 另注：HyperDbg 的 `HvSetNmiExiting` 会**同时置 `ACKNOWLEDGE_INTERRUPT_ON_EXIT`**；本项目 `acknowledge_interrupt_on_exit = true` 而 `external_interrupt_exiting = false`（组合上无害，但需理解其语义）。

---

## 3 本项目在开源界的位置

把十个维度合起来看，本项目的组合是：

> **EPT 每核独立（对） + 同步全局化（错） + 根态文件 IO（独有） + 自建 host 页表（少数派） + 无上限跨核忙等（独有） + 遇 pending 静默丢弃（独有） + 一律 KeBugCheckEx（少数派） + 能力 MSR 统一收敛（对） + 每核建 EPT 处理大小核差异（加分）**

**这个组合在开源界没有对应物。** 具体地：

| 维度 | 本项目 | 开源界主流 | 判定 |
| --- | --- | --- | --- |
| A EPT 数据 | per-vCPU ✅ | per-vCPU（多数） | **符合** |
| A' 同步模型 | 全局锁 + 广播 | per-vCPU 派无跨核锁 | **独有错配** |
| B 根态约束 | 文件 IO + 池耗尽分配 | 禁 API/禁 IO/预分配耗尽即失败 | **偏离** |
| C UAF 处理 | 引用计数 + 忙等 | 固定数组免 UAF / 延迟释放 | **偏离** |
| D MTF | 拆两处，配对可失败 | 同函数紧邻 + 防重入 | **偏离** |
| E 跨核刷新 | 广播 + 回调做重活 | 本地刷新 / 延迟 request | **偏离（代价）** |
| F 锁 | 锁内重活 + 非 VM-exit 上下文使用 | 锁内只改元数据 | **偏离** |
| G 失败处理 | 一律蓝屏 | 分层（guest 侧不蓝屏） | **偏离** |
| H 代际 | 能力 MSR 收敛 | 同 | **符合** |
| H' 大小核 | 每核建 EPT | 多数未处理 | **加分** |
| I Host 页表 | 自建 + 512GB 直映 | 不自建 | **少数派** |
| J 事件注入 | 静默丢弃 | 显式 pending 队列 | **偏离** |

---

## 4 三条「不用锁」范式（直接对应本项目的 0x101 通路）

开源界解决同类问题的三条范式，正好一一对应本项目 0x101 的三条主通路：

### 范式 1：per-vCPU 私有 EPT ——「各改各的」

- **`jonomango/hv`**：`vcpu_ept_data ept` 内嵌 per-vCPU，用 `_readfsbase_u64()` 定位；所有 EPT 操作只碰本核数据。
- **`NoirVisor`**：`nvc_ept_protect_hypervisor` 遍历 `hvm->virtual_cpu[i]` **各改各的**，而不是「改一份共享表再广播」（`vt_core/vt_ept.c`）。
- **`HyperPlatform` / `hvpp` / `MiniVisorPkg` / `SimpleVisor` / `HyperBone` / `HyperDbg` / `HFS` / `HyperWin` / `kHypervisor`**：全部 per-vCPU/per-processor。

→ **对应本项目 0x101 的 P1（锁内重活）与 P9（IPI_LEVEL 广播）**。本项目的数据模型**已经是** per-vCPU，只需把同步模型对齐即可。

### 范式 2：固定数组 + 只摘链不释放 ——「结构性免 UAF」

- **`jonomango/hv`**：`hooks.buffer[capacity]` + `active_list_head`，节点只摘链不释放 → **跨核修改根本不存在共享写，也就不需要锁、不需要广播、不需要等待**（`hv/ept.cpp`）。
- **`HyperDbg`**：延迟释放池（`"deallocated on next IOCTL"`）。

→ **对应本项目 0x101 的 P7/P8（引用计数 + 无上限忙等 + 与事务锁互相等待）**。
比本项目的「引用计数 + 忙等」**更简单且更安全**——因为它把「等待引用归零」这个动作**彻底消掉了**。

### 范式 3：请求 + 惰性处理 ——「从不等待」

- **KVM**：`kvm_make_request(KVM_REQ_TLB_FLUSH)` + `kvm_vcpu_kick()`；各核在 `vcpu_enter_guest()` 进 guest 前 `kvm_check_request()` → `kvm_vcpu_flush_tlb_all()`。`Documentation/virt/kvm/vcpu-requests.rst` 明确「刷新被延迟到该核下次进 guest」。
- **Xen**：`defer_flush` + `ept->invalidate` cpumask + `vmx_vmenter_helper()`。
- **ACRN**：`ept_flush_guest()` 只置 `vcpu_make_request(ACRN_REQUEST_EPT_FLUSH)`。
- **Hyper-V**：目标 TLB "locked" 时**挂起调用者并重发**，用调度替代忙等。

→ **对应本项目 0x101 的 P12（卸载路径跨核等待）与 P9**。

**映射到本项目的具体方案**（本项目 EPT 每核独立，但 watch 表全局、安装时会从当前核循环改所有核）：

1. 在每个核的 `__vcpu` 状态里加 **`EptFlushPending` 位** 与 **`mode` 字段**（`InGuest` / `Exiting` / `Outside`）。
2. **写者只做三件事**：更新全局 watch 表（带写屏障）→ 改本核 EPT 并本核 `invept` → 对其它核置 `EptFlushPending`，按 `mode == InGuest` 决定是否发 IPI。**不等待**。
3. **读者在进 guest 前检查**：在 `VMRESUME` 之前、关中断之后的位置检查该位，置位则执行 `invept` 并清位（即 ACRN 的 *"handled before vcpu vmenter"*、Xen 的 `vmx_vmenter_helper()`）。
4. **防漏 IPI 用 Dekker 模式**：本核 `_disable()` → 置 `mode=InGuest` → `_mm_mfence()` → 读 `EptFlushPending`；发起核置位 → `_mm_mfence()` → 读 `mode==InGuest` 才发 IPI。**对不在 guest 的核一个 IPI 都不发。**
5. **若语义要求「安装返回即全核生效」**：只等待一个 **ack 计数**（各核补做完置 ack）+ 超时；未 ack 的核由它自己下次 VM-entry 补做——**因为 request 幂等且惰性，晚做只影响窗口长度、不影响正确性**。这比等 DPC 执行体有界得多。

> Windows 与 Linux 的差异：Windows 没有「vCPU 线程」，request 位要挂在 per-CPU 的 VMCS 上下文里；`mode = InGuest` 的置位必须在关中断之后、且是进 guest 前最后一步。

---

## 5 可直接采纳的做法（按收益排序）

| # | 采纳项 | 来源 | 消除的本项目 0x101 通路 |
| --- | --- | --- | --- |
| 1 | **固定数组 + 只摘链不释放**，取代引用计数 + 忙等 | `jonomango/hv` | **P7、P8** |
| 2 | **请求 + 惰性刷新**（`EptFlushPending` + `mode` + Dekker），取代 `KeGenericCallDpc` 同步广播 | KVM / Xen / ACRN | **P9、P12** |
| 3 | **per-vCPU 私有 EPT 范式贯彻到底**（跨核操作改逐核定向提交） | `NoirVisor` / `jonomango/hv` | **P1、P9** |
| 4 | **MTF 置位与恢复点登记同函数紧邻 + 防重入断言** | DdiMon（`NT_ASSERT`）/ HyperDbg（三态幂等） | **P10** |
| 5 | **根态日志用 `_SAFE` 变体**（不 DbgPrint、不写文件），或加「根态标志」门闸 | HyperPlatform | **P3** |
| 6 | **池耗尽即失败**（bugcheck 或返回错误），禁止隐式分配 | HyperPlatform / kHypervisor / HyperBone | **P4** |
| 7 | **锁内只改元数据**，flush/重活移出锁外 | KVM / Xen / ACRN / jonomango-hv | **P1、P5** |
| 8 | **显式 pending 事件队列**，取代「遇 pending 静默 return」 | hvpp（64 深度 FIFO）/ HyperDbg / jonomango-hv | 事件丢失 |
| 9 | **guest 侧失败不升级为宿主蓝屏**（分层：初始化期/guest 侧记录+清理，仅宿主侧不可恢复才 fail-fast） | HyperPlatform / hvpp / SimpleVisor | — |
| 10 | **DPC 回调里只发 VMCALL，重活留给 root** | HyperDbg | 降低 P9 代价 |
| 11 | **复用 System process CR3 或用 1 页预留 VA 改 PTE，不自建 host 页表** | MiniVisorPkg / hvpp / HyperDbg / HyperPlatform | **P3、P5** + Host PCID 别名 |

---

## 6 应当拒绝的做法（反面教材）

| 反面做法 | 出处 | 为什么拒绝 |
| --- | --- | --- |
| 全局单份 EPT + 无同步 | `TinyVT`（`Eptp EptP` 全局，每核写同一 EPTP） | 多核竞态 |
| 卸载后立即 `kfree`，无 INVEPT | `TinyVT` | UAF + 脏 TLB |
| **VM-exit 内持全局 `spin::Mutex` + 分配内存** | `illusion-rs`（`ept_violation.rs` 持 `SHARED_HOOK_MANAGER.lock()`；`hooks/memory_manager.rs` 在 exit 内 `box_zeroed`） | **正是本项目应避免的形态，也是它现在的问题** |
| 卸载靠 `UtilSleep(1000)` 粗糙延时 | `DdiMon`（README 自认 "not robust, production quality software"） | 非确定性等待 |
| MTF handler 未实现 | `TinyVT` | 权限不恢复 |
| 无条件覆盖 pending 注入字段 | `TinyVT`（`ExitExceptionOrNmi` 不查 pending） | 事件丢失 |
| 硬编码 `PageWalkLength` / memory_type | `HyperBone` / `ZeldaOS` | 代际兼容风险 |
| `ASSERT` fail-fast 处理 EPT misconfig | `ACRN` | guest 故障升级为宿主崩溃 |

---

## 7 一句话总结

> **本项目的 EPT 数据模型与开源界主流一致（per-vCPU，这是对的，且它每核独立建表还额外解决了大小核能力差异——这一点比多数开源项目做得好）。**
>
> **真正的问题不在「数据模型」，而在「同步模型」：它把 per-vCPU 的数据当成了全局共享资源，于是发明了全局事务锁 + IPI 同步广播 + 无上限跨核忙等——这三样在开源界几乎没有对应物。**
>
> **开源界给出的替代方案已经成熟且更简单：`jonomango/hv` 的「固定数组 + 只摘链不释放」免掉引用计数与等待；KVM/Xen/ACRN 的「请求 + 惰性刷新」免掉同步广播与忙等；而 per-vCPU 私有 EPT 范式本身就是本项目已有的数据模型。**
>
> **采纳这三条，本项目的 11 项同步机制可收敛到 2 项，0x101 的 12 条通路中 9 条消失。**

---

## 8 依据（可溯源）

- `tandasat/HyperPlatform`：`ept.cpp`（`EptData` per-processor、`kEptpNumberOfPreallocatedEntries=50`、`HYPERPLATFORM_COMMON_BUG_CHECK`）、`vm.cpp`（`VmpAdjustControlValue`、`VmpLaunchVm`）、`vmm.cpp`、`util.cpp`（`PTE_BASE` 特征码）、`log.h`（`_SAFE` 变体）
- `tandasat/DdiMon`：`shadow_hook.cpp`（`ShpEnablePageShadowingForRW` / `ShpSetMonitorTrapFlag` / `ShpSaveLastHookInfo` / `ShpRestoreLastHookInfo` / `NT_ASSERT`）、`ddi_mon.cpp`（`UtilSleep(1000)`）
- `tandasat/MiniVisorPkg`：`HostMain.c:HandleVmExit` 头注释、`WinHostInitialization.c`、`MemoryManager.c`、`MemoryAccess.c`（`ReserveVirtualAddress`/`MapPa`）
- `wbenny/hvpp`：`lib/spinlock.h`（`max_wait=65536`）、`lib/win32/mp.cpp`、`hypervisor.cpp`、`vcpu_t::interrupt_queue_t`
- `ionescu007/SimpleVisor`：`shv_x.h`（`SHV_VP_DATA`）、`shvvmx.c`
- `HyperDbg/hyperdbg`：`vmm/ept/Ept.c`（`EptLogicalProcessorInitialize`、`EptSetPML1AndInvalidateTLB`）、`vmm/ept/Invept.c`、`vmm/vmx/Mtf.c`、`broadcast/Broadcast.c`、`broadcast/DpcRoutines.c`、`hooks/ept-hook/EptHook.c`（延迟释放池）、`vmm/vmx/Hv.c`（`HvSetNmiExiting`）
- `SinaKarvandi/Hypervisor-From-Scratch`：`Ept.c`（`EptPageHook`/`EptPageUnHookSinglePage`/`EptCheckFeatures`）、`Spinlock.c`、`Exit.c`
- `hititou/VMXHypervisorToolbox`：`docs/audit/intel-vmx-code-audit.md`、`docs/per_cpu_pte_hook_execution_flow.md`
- `DarthTon/HyperBone`：`Hooks/PageHook.c`、`VmxExitHandlers.c`、`EPT.c`
- `jonomango/hv`：`hv/ept.cpp`、`hv/spin-lock.h`、`exit-handlers.cpp`
- `Zero-Tang/NoirVisor`：`vt_core/vt_ept.c`、`vt_exit.c`、`vt_ept.h`
- `memN0ps/illusion-rs`：`intel/ept.rs`、`intel/vmexit/ept_violation.rs`、`hooks/memory_manager.rs`、`allocator.rs`
- `WWFWT/TinyVT`：`InitVMCS.cpp`、`EptHook.cpp`、`VmExitHandler.cpp`
- `amiryeshurun/HyperWin`：`vmm/vmm.c`、`vmx_modules/hooking_module.c`
- `KelvinMsft/kHypervisor`：`vmm.cpp`、`vm.cpp`
- KVM：`arch/x86/kvm/vmx/vmx_ops.h`（`__invept`）、`arch/x86/kvm/mmu/mmu.c`、`arch/x86/kvm/x86.c:vcpu_enter_guest`、`Documentation/virt/kvm/vcpu-requests.rst`、`arch/x86/kvm/vmx/capabilities.h`、`arch/x86/kvm/vmx/posted_intr.c`
- Xen：`xen/arch/x86/mm/p2m-ept.c`（`ept_sync_domain`）、`xen/arch/x86/hvm/vmx/vmx.c`（`vmx_vmenter_helper`）
- ACRN：`hypervisor/arch/x86/guest/ept.c`（`ept_flush_guest`、`ept_misconfig_vmexit_handler`）
- Microsoft：`HvCallFlushVirtualAddressSpace` 文档、Bug Check 0x101、`KeIpiGenericCall`、`KIPI_BROADCAST_WORKER`
