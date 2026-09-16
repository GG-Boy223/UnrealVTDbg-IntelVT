# 01 VT_Driver 逻辑梳理

VT_Driver 是 VMX root 态核心，约 1.8 万行。以下按启动、Host 环境、VMCS、EPT、VM-exit、CR3、释放策略分组。

## 1 启动与初始化链

Driver/DriverLifecycle.cpp 中 DriverEntry 的顺序为：

1. InitNtoskrnlSymbolsTable()：从 ntoskrnl PDB 解析必需符号。任一必需符号缺失时返回 FALSE，驱动干净失败（Init/Symbolic/InitNtoskrnl.cpp）。
2. virtualization_support() 与 required_vmx_capabilities_supported()：读 IA32_FEATURE_CONTROL 与 VMX 能力 MSR，只做能力判定，不额外设门槛。
3. InitGlobalVariables()：分配 vcpu 数组、MSR 位图、IO 位图、EPT 状态等。
4. vmm_init()：逐个逻辑处理器亲和执行 initalize_vcpu。
5. 失败路径：DriverEntryFailure 加 disable_vmx_operation 加 free_vmm_context；free_vmm_context 各段判空且 lifecycle 状态已置位，重复调用是幂等的。

## 2 每个逻辑处理器的启动

Vmm/VmmEntry.cpp 按 CPU 亲和调用 VmmProcessor 的 init_logical_processor2，顺序为：

1. adjust_control_registers
2. VMXON
3. VMPTRLD
4. prepare_external_structures
5. ensure_host_page_tables_for_vcpu
6. validate_host_page_tables_for_vcpu
7. fill_vmcs
8. VMLAUNCH

任一步失败都会执行 vmxoff_after_failed_initialization 回滚；vmm_init 失败时上层执行 vmoff(iter) 与 free_vmm_context。

## 3 Host 执行环境（Globals/HostTables.cpp，约 860 行）

- Host CR3：KeStackAttachProcess 到 dwm.exe 后读取 CR3。系统必须存在 dwm.exe。
- 物理内存直映：HOST_FIRST_ADDRESS 到 PML4 的 host_physical_memory_pml4_idx 槽，HOST_PHYSICAL_MEMORY_PD_COUNT=512，覆盖 512GB。
- VM-exit 关键页（驱动镜像、KPCR、vcpu、IO 位图、EPT 各结构）用 ensure_host_virtual_range 到 map_host_virtual_page 动态克隆 PDPT/PD/PT 并补 PTE；池容量 HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT=1024 页表页，大页分支提前返回视为成功。
- 补映射后不做 TLB 失效，理由是这些映射都是 not-present 变 present 的新增项，硬件不缓存不存在的翻译。
- 若关键页无法映射，则禁止 VMLAUNCH（安全设计，非缺陷）。

## 4 VMCS 控制位（Vmcs/VmcsControls.cpp、VmcsFill.cpp）

- _MINIMAL 宏在 vmcs.h 与 VmcsFields.h 无条件定义，因此当前为精简模式：invlpg_exiting、rdtsc_exiting、mov_dr_exiting、wbinvd_exiting、use_io_bitmaps 均为 false；MSR 位图只拦截 IA32_FEATURE_CONTROL。
- 统一收敛：ajdust_controls(ctl, msr) 执行 ctl = (ctl 与 MSR.high) 或 MSR.low，并在 IA32_VMX_BASIC.true_controls 指示存在 TRUE 系列 MSR 时套用 TRUE_PIN/PRIMARY/SECONDARY/EXIT/ENTRY_CTLS。不支持的特性位会被掩码清除，这是同一路径覆盖 8 至 14 代、X 系列、Ultra 的关键，无需增加进入门槛。
- 异常位图全 0：不拦截异常，仅靠 IDT-vectoring 重注入；注入前用 vm_entry_event_pending 防止覆盖已有事件。
- VPID：guest_vpid 为 1 全局共用，每个 vCPU 有独立 EPTP；CR3 写用 invvpid single-context-except-global，CR4 变化用 invvpid single-context。多 vCPU 共用同一 VPID 且 EPTP 不同是合法组合。
- 控制位初始状态：CR3-load/store exiting 初始关闭，需要时由 VmcallDispatch 触发的 Cr3TrackerSynchronizeExitPolicy 打开 CR3-load（Common/Ring0/Hvm/Cr3ExitPolicy.h），并用 CR3_TARGET_VALUE0 到 3 抑制多余退出。

## 5 EPT

- 每个逻辑处理器一份完整 EPT（__ept_state 挂在 __vcpu 上，Vmm/VmmVcpu.cpp 的 init_vcpu() 只分配资源，init_vcpu_ept() 在目标核上逐核建表），互不共享页表。
- 覆盖范围按 max(RAM 上界, 全平台最大物理地址上界) 计算，再由 Common/Ring0/Hvm/EptCoveragePolicy.h 裁剪到 4 级 EPT 的 2^48(256TB) 硬上限；不再只按 RAM 上界取整（旧逻辑在 ≤512GB 内存的机器上只覆盖 0..512GB，来宾访问高地址 MMIO 会走到未处理 violation 的 64 次上限并蓝屏）。
- 全平台最大物理地址上界由 Vmm/VmmContext.cpp 的 query_platform_physical_address_limit() 在 vmm_init() 开始时逐核读 CPUID.80000008H 取最大值（Vmm/VmmEntry.cpp 调用）：MMIO/BAR 是操作系统按全平台解码能力分配的，按单核较小的 MAXPHYADDR 建表会让该核访问高地址 BAR 时命中未映射条目。
- EPT 页表由所属逻辑处理器自己构建：IA32_VMX_EPT_VPID_CAP（1GB 大页支持）、CPUID.80000008H、MTRR 都是每逻辑处理器状态；混合架构（P-core + E-core）上大小核能力可能不同，在一个核上替所有核统一建表会写出该核不支持的条目（例如 1GB EPT 大页 → EPT misconfiguration）。构建点位于 Vmm/VmmProcessor.cpp 的 init_logical_processor2()，在 VMXON 之前、Host 页表补映射与 fill_vmcs 之前。
- 物理内存范围来自 MmGetPhysicalMemoryRanges() 缓存；含 RAM 的 1GB 区域建 PD 页并用 2MB 大页映射（RAM 取 MTRR 类型，区域内非 RAM 的 2MB 页取 UC），完全不含 RAM 的 1GB 区域构建期只留 not-present 的 PDPTE。
- MTRR 数据仍全局读一次（Ept/EptMtrr.cpp build_mtrr_map()）：MTRR 由固件/操作系统在各核上统一编程，Intel 平台上同包内取值一致，与 HyperPlatform（EptInitializeMtrrEntries 只在初始化时读一次）的处理方式相同。
- 非 RAM 区域的按需映射：首次访问触发 EPT violation 时由 ept::ensure_mmio_mapping() 补映射 —— CPU 支持 1GB EPT 大页（IA32_VMX_EPT_VPID_CAP[17]）时填一个 1GB UC 条目，否则从构建期预分配的 PD 页池（EPT_MMIO_PD_POOL_PAGE_COUNT）取一页填 512 个 2MB UC 条目；该函数只做纯内存写入（VM-exit 里禁止文件 IO 与内存分配），统计在 cleanup_page_table() 的安全上下文输出。
- 页表构建在 Ept/Build/EptPageTables.cpp，运行期大页拆分在 Ept/Build/EptRuntimeSplit.cpp；地址换算在 Ept/Build/EptAddressing.cpp，带 kHostPhysicalLimit 边界检查。
- pml2_pages[i] == nullptr 表示该 1GB 区域由 PDPTE 大页覆盖（含 RAM 区域的常规 PD 页之外，或按需补映射的 1GB 页），hook/watch 安装、MTRR 刷新与 Host 页表登记都必须先判空跳过。
- EPT violation 处理在 VmExit/VmExitEpt.cpp：先 acquire_page_hot_reference 取热引用，处理完 release_page_hot_reference；已核对 handled_return 与 unhandled_after_release 两条路径不存在双重释放与空指针解引用。
- 未处理 violation 的累计规则：只有连续相同的 GPA 与 qualification 才累加，达到 64 次后 vmx_fatal_error(vmx_fatal_unhandled_ept_violation)。

## 6 EPT Hook 与 Watch

- Hook 生命周期文件：Ept/Hook/Lifecycle 下的 EptHookCc、EptHookDefault、EptHookVmcall、EptHookInt1 与 Ept/Hook/Cleanup。
- 跳板缓冲区统一使用 kTrampolineBufferSize 常量；指令长度解码使用有界 LDE（Ept/Hook/Instruction/EptHookInstruction.cpp）。
- watch 表容量 EPTWATCHLISTSIZE=256，表项状态为 FREE、INSTALLING、ACTIVE、REMOVING；热引用计数与 removing 标志用于防止释放后使用。
- MTF pending 状态机在 Globals/MtfPendingState.cpp：mark_pending_mtf_page 与 clear_pending_mtf_metadata，配合 mtf_reference_count 处理同一 CPU 上 pending 页被替换的情形。
- INT3 隐藏：watch 页加伪页；伪页内容按需整页同步（fake_page_needs_full_sync），再在伪页写入 0xCC，真实物理页不被改写。

## 7 VM-exit 分发

VmExit/VmExitDispatcher.cpp 按 exit reason 分发表驱动分发，主要处理器：

- VmExitControl.cpp：MOV CR0/CR3/CR4、CLTS、LMSW。CR0/CR4 写校验保留位与非法组合，写 CRx_READ_SHADOW 与 GUEST_CRx 并套用 IA32_VMX_CRx_FIXED0 与 FIXED1；CR3 写处理 PCID 首次开启要求 CR3.PCID 为 0。
- VmExitMsr.cpp：PAT 与 EFER 走 VMCS 字段（EFER 写值必须与 GUEST_EFER 一致，否则注入 #GP）；MTRR 相关写后调用 update_ept_memory_type 并 INVEPT；未知 MSR 用 rdmsr_safe/wrmsr_safe 后注入 #GP。
- VmExitInstruction.cpp、VmExitIo.cpp、VmExitDebugRegisters.cpp、VmExitEventInjection.cpp、VmExitException.cpp。
- MTF 处理：vmexit_monitor_trap_flag_handler 按 pending 页的 Options 分支恢复权限（函数 Hook 恢复不可执行，watch 恢复组合权限并可选注入单步）。
- NMI 处理：vmexit_nmi_window_handler 在 queued_nmis 非空且没有 pending event 时注入 NMI，并维持或关闭 NMI-window exiting。

## 8 CR3 跟踪

- Cr3/Cr3Tracker.cpp 等：128 槽位，自旋锁保护，PML4 fingerprint 校验，generation 递增防 ABA。
- NormalizeCr3 要求 root 非 0 且小于 512GB。
- 地址换算使用 host_physical_memory_base 加物理地址直读页面完成页表遍历，边界检查齐全。
- 后台 worker 线程已废弃（Cr3/Cr3TrackerWorker.cpp 中明确说明），改为 VM-exit 命中时同步 Observe。

## 9 释放策略与 fatal 路径

- Common/Ring0/Hvm/VmmReleasePolicy.h：CanRelease 要求 allVcpusQuiescent 且状态不是 running 或 stopping，否则触发 vmx_fatal_error(vmx_fatal_release_active_context)。
- vmx_fatal_error 实现在 VmExit/VmExitCommon.cpp，最终 KeBugCheckEx。这是 fail-fast 设计：宁可蓝屏也不带病继续跑。

## 10 精简模式下的未激活路径

- hide_vm_exit_overhead() 在精简模式下不被使用。
- rdtsc、rdtscp、invlpg、invd、mov_dr 等 handler 在精简模式下不会被触发，属于预留实现。
- 这些不是缺陷，是作者刻意的性能取舍；切到完整模式需要重新验证一遍。

## 11 已验证的关键不变量

1. eptWatchList_lock 的全部加锁点都在 root 态：Ept/Watch 下各文件、Globals/MtfPendingState.cpp、Globals/GuestThreadState.cpp、Cr3/Cr3Tracker.cpp、VmExit/VmExitEpt.cpp、VmExit/VmExitException.cpp。
2. 调用链证据：Driver/DriverGuestCommands.cpp 的 SetBreakpoint 与 RemoveBreakpoint 只被 VmCall/VmcallDispatch.cpp 调用；GuestThreadState 的 clear_breakpoint_state 同理；Cr3TrackerObserve 只被 VmExit/VmExitEpt.cpp 调用。
3. 因此不存在 root 态自旋等待 guest 态持锁导致的死锁。
