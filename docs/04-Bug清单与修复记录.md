# 04 Bug 清单与修复记录

记录本次逐行扫描中确认并修复的问题。每条包含文件与行、原问题、原因、修法与验证方式。

## 严重（会导致蓝屏或数据错乱）

### B01 VM-exit 原因位误判，破坏 NMI 阻塞语义

- 文件：VT_Driver/VmExit/VmExitDispatcher.cpp（原 151 到 160 行附近）。
- 原问题：把 exit reason 的 bit31 当成 NMI unblocking 标志，并直接清除 GUEST_INTERRUPTIBILITY_STATE 的 NMI blocking 位。
- 原因：Intel SDM Vol3C 中 VM_EXIT_REASON 的 bit31 是 VM-entry failure 标志；NMI unblocking due to IRET 实际位于 VM-exit interruption information 与 EPT violation exit qualification 的 bit12，且方向应为置位而不是清零。
- 影响：guest 会提前收到 NMI，破坏中断嵌套；同时掩盖了 VM-entry failure 的判定。
- 修法：删除错误分支，新增 recover_nmi_blocking()，按 SDM 27.7.1.2 与 KVM vmx_recover_nmi_blocking 的规则，在 pin-based virtual NMI 启用、IDT-vectoring 无效、且确实置位 nmi_unblocking 时，把 blocking_by_nmi 置 1；VM-entry failure 分支只记录日志交给致命路径。
- 验证：VT_Driver Release x64 编译 0 错误 0 警告。

### B02 读取隐形软件断点未加事务锁，可能与删除事务竞态

- 文件：VT_Driver/VmCall/VmcallDispatch.cpp（原 379 到 381 行）。
- 原问题：VMCALL 读取隐形断点直接调用 ept::get_hide_software_breakpoint，未持有 eptWatchTransaction_lock。
- 原因：该函数内部遍历 hooked_page_list，遍历不持 hooked_page_list_lock，依赖安装与删除事务互斥；并发删除事务可能在遍历过程中回收页面记录。
- 修法：新增 ExecuteReadSoftwareBreakpoint()，在调用前后加解 eptWatchTransaction_lock，并做 vcpu 与参数判空。
- 验证：编译通过；锁顺序与既有安装/删除路径一致，无新增嵌套。

### B03 EPT PML2 页发布时尚未填满大页模板

- 文件：VT_Driver/Ept/Build/EptPageTables.cpp（原 409 到 463 行附近）。
- 原问题：先分配 pml2_pages 数组元素，之后再用另一个循环填充 PDE 模板；两者之间存在窗口。
- 原因：分配与初始化分离，若此时发生 EPT 遍历，会看到未填充的 PD 页，触发 EPT misconfiguration。
- 修法：把模板填充移入分配循环，用 __stosq 一次性填写 512 项，保证 pml2_pages[i] 一旦发布即为完整条目。
- 验证：编译通过；地址换算与既有大页拆分逻辑不变。

### B04 跳板安装时无界 LDE 解码

- 文件：VT_Driver/Ept/Hook/Instruction/EptHookInstruction.cpp。
- 原问题：while 循环累加 LDE 解码长度，未校验解码失败、未校验指令长度上限、未校验是否跨页、未校验跳板缓冲区是否装得下。
- 原因：运行在 VM-exit 上下文，无界解码会让整个物理 CPU 无法回到宿主。
- 修法：新增 decode_hooked_instructions_size()，校验 LDE 返回 0、单条长度超过 15 字节、覆盖范围跨出 4KB 页、覆盖字节加回跳跳转超过 kTrampolineBufferSize，任一失败即返回 false，调用点在改写伪造页之前执行解码。
- 验证：编译通过；Hvpp 风格的有界校验与 Intel SDM Vol2 的 15 字节上限一致。
### B05 跳板缓冲区容量硬编码，申请与写入不一致

- 文件：VT_Driver/Ept/EptHook.h 与 Ept/Hook/Lifecycle 下 EptHookCc、EptHookDefault、EptHookInt1、EptHookVmcall 四个文件。
- 原问题：pool_manager::request_pool 使用字面量 100，跳板写入上限也是另一处字面量 100，两处不一致时存在越界风险。
- 修法：在 EptHook.h 定义 kTrampolineBufferSize 与 kAbsoluteJumpSize，四个生命周期文件统一改用该常量。
- 验证：编译通过。

### B06 监视命中路径缺少 vcpu 判空

- 文件：VT_Driver/Ept/Watch/EptWatchEvents.cpp（原 279 行）。
- 原问题：直接写 vcpu->breakpoint_detected，未判空。
- 修法：加 vcpu 判空后再写。
- 验证：编译通过。

### B07 31 位位域按 %X 传参

- 文件：VT_Driver/Globals/VmxState.cpp（原 22 行与 100 行）。
- 原问题：vmx_basic.vmcs_revision_id 是 31 位位域，直接传给可变参数日志函数。
- 原因：位域按整型提升传给可变参数时高位内容由实现决定，日志可能出现垃圾值。
- 修法：显式 static_cast<unsigned int> 收敛为 32 位宽。
- 验证：编译通过。

### B08 复合递减表达式触发越界告警

- 文件：VT_Driver/Driver/VtLoadLog.cpp（原 52 行）与 Common/Ring0/Comms/SerialPortFormat.cpp（原 52 行同名模式）。
- 原问题：temp[--temp_offset] 在同一表达式中对 size_t 递减并取值。
- 修法：拆成先自减再取值两句。
- 验证：编译通过；行为等价。

### B09 switch 分支缺少 break

- 文件：Common/Ring0/Comms/SerialPortFormat.cpp（原 198 行后）。
- 原问题：case 分支结束后缺少 break，发生非预期贯穿。
- 修法：补 break。
- 验证：编译通过。

### B10 MSR 位图成员名错误导致非精简模式编译失败

- 文件：VT_Driver/Vmcs/VmcsFill.cpp（原 26 行）。
- 原问题：memset(vcpu->vcpu_bitmaps.msr_bitmap, 0xff, PAGE_SIZE)，但 __vcpu_bitmaps 结构体没有 msr_bitmap 成员。
- 原因：该行只在未定义 _MINIMAL 时参与编译，当前构建恰好是精简模式，因此问题被掩盖，一旦切到完整模式立即编译失败。
- 修法：改为 memset(&vcpu->msr_bitmap, 0xff, PAGE_SIZE)，指向真正按页对齐的 4KB MSR 位图对象。
- 验证：VT_Driver Release x64 编译 0 错误 0 警告；位图布局与 HypervisorBitmaps.cpp 的低/高区索引一致。

### B11 源文件缺少 UTF-8 BOM

- 文件：VT_Driver/Cr3/Cr3TrackerInternal.h。
- 原因：仓库内含中文的源文件统一为 UTF-8 with BOM，缺 BOM 时 MSVC 可能按本地代码页解析中文字面量。
- 修法：补 BOM。
- 验证：编译通过。

### B12 必需符号检查被调试宏关闭（Win10 与 Win11 两套驱动）

- 文件：DbgkSysWin10/Core/Driver.h、DbgkSysWin11/Core/Driver.h、DbgkSysWin10/Init/Symbolic/InitSymbolic.cpp、DbgkSysWin11/Init/Symbolic/InitSymbolic.cpp。
- 原问题：CHECK_FUNC_PTR 只 DbgPrint，不阻断；且 Win11 侧检查位于 DEBUG 内、Win10 侧位于 DEBUG0 内，Release 下永远不会执行。
- 影响：PDB 符号缺失时驱动仍继续初始化，随后在调用点以 NULL 函数指针调用，直接蓝屏且无法定位。
- 修法：Core/Driver.h 增加 volatile LONG g_MissingSymbolCount，CHECK_FUNC_PTR 改为 InterlockedIncrement 计数；新增 CHECK_OPTIONAL_FUNC_PTR 供可选符号使用；InitSymbolic 在 Release 下也无条件执行 CheckFunctionPointers，缺失必需符号时返回 INIT_SYMBOLS_STATUS_MISSING_SYMBOLS 并中止初始化。
- 验证：两套驱动 Release x64 编译 0 错误。
### B13 可选符号未判空导致 NULL 调用

- 文件与位置：
  - DbgkSysWin11/Process/process.cpp 第 70 到 76 行：PspInheritSyscallProvider。
  - DbgkSysWin11/DbgkApi/Api/DebugThreadCreate.cpp 第 278 到 282 行与第 314 到 328 行：PsWow64GetProcessNtdllType 与 MiReferenceControlAreaFileWithTag。
  - DbgkSysWin10/DbgkApi/Api/DebugThreadCreate.cpp 第 135 到 139 行：PsWow64GetProcessNtdllType。
- 原因：这些符号由 PDB 运行时解析（ModuleExtender::GetPointer 查不到即返回 nullptr），不同系统版本是否存在并不一致。
- 修法：
  - PspInheritSyscallProvider 缺失时按成功处理，保持原有语义不变。
  - PsWow64GetProcessNtdllType 缺失时退化为只处理索引 0（64 位 ntdll），保留每个进程只发一次 ntdll 镜像通知的语义。
  - MiReferenceControlAreaFileWithTag 缺失时回退到必需符号 MiReferenceControlAreaFile。
- 验证：Win10 与 Win11 驱动 Release x64 编译 0 错误（Win10 侧有 1 个既有 C4996，ExAllocatePoolWithTag 已弃用，非本次引入）。

### B14 新增缺失符号状态码

- 文件：Common/Shared/DriverProtocol.h（原 25 行后）。
- 修法：新增 INIT_SYMBOLS_STATUS_MISSING_SYMBOLS 0xE107U，让用户态能区分 符号缺失与其它初始化失败。
- 验证：编译通过。

### B15 维护性调整（无语义变化）

- VT_Driver/Globals/HostTables.cpp 第 791 行：ensure_host_virtual_range 调用写成单行，便于审阅。
- VT_Driver/Globals/GlobalsHvApi.h 第 140 行：PFN_PSGETNEXTPROCESS 的参数标注改为 _In_opt_，与调用点允许传空的语义一致。

### B16 EPT 只覆盖 0..512GB，来宾访问高地址 MMIO 触发 Arg1=7 蓝屏（Ultra 9 275HX）

- 现象：i 系列平台运行正常；Ultra 9 275HX（Arrow Lake-HX，Win11 26100，24 逻辑核）进入 VT 后数分钟整机蓝屏，代码 0x00020001。
- 转储证据（F:\001\VT\蓝屏文件\Ultral9 275hx，本机首手复核）：
  - 091526-15453-01.dmp：Bugcheck code 00020001，Arguments 00000007 / 00000001 / 000003ffaf000c60 / 00000d81。
  - 091526-14343-01.dmp：同一 Bugcheck，Arg2（处理器号）= 0x17 = 23，Arg3、Arg4 与第一份完全相同。
  - 栈：nt+0x4f9280（KeBugCheckEx）← VT_Driver+0x1c849（vmx_fatal_error）← VT_Driver+0x1b5f4（vmexit_ept_violation_handler）← VT_Driver+0x1950e。
- 参数解码：Arg1=7 对应 VT_Driver/VmExit/VmxFatal.h 的 vmx_fatal_unhandled_ept_violation；Arg3=0x3FFAF000C60 是 GPA≈4.0TB；Arg4=0xD81 按 __ept_violation 位域解码为 bit0=1（数据读）、bit3/bit4/bit5=0（EPT 条目读/写/执行权限全为 0，即条目不存在）、bit7/bit8=1（GLA 有效且来自线性地址翻译）、bit9=0（内核态），符合 MmMapIoSpace 映射显卡 BAR 的访问特征。
- 根因：VT_Driver/Ept/Build/EptPageTables.cpp 的 query_required_pdpt_count() 只用 MmGetPhysicalMemoryRanges() 的 RAM 上界算覆盖范围，再被 Common/Ring0/Hvm/EptCoveragePolicy.h 的 minimum_pdpte_count=512 抬底，因此 ≤512GB 内存的机器 EPT 只覆盖 0..512GB。本机显卡/PCIe MMIO 窗口位于 ≈4TB（PML4 索引 7、PML3 索引 510、flat PDPTE 4094），而页表只填了 PML4[0]，条目不存在 → EPT violation → vmexit_ept_violation_handler 的 unhandled 分支只 INVEPT 后重试 → 连续 64 次（kMaxUnhandledEptViolationRetries）后 vmx_fatal_error → 蓝屏。两份转储 GPA 与 qualification 完全相同、处理器号不同，属确定性触发；i 系列平台 MMIO 窗口在 512GB 以内，所以从未暴露。
- 修法（分支 assistant/ept-coverage-fix，基线 20ee6bb）：
  1. EptPageTables.cpp:129 query_required_pdpt_count() 把覆盖范围抬到 max(RAM 上界, 1<<CPUID.80000008H.MAXPHYADDR)，并打印 MAXPHYADDR；EptCoveragePolicy.h:15 新增 four_level_limit_bytes，超过 4 级 EPT 的 2^48(256TB) 硬上限时裁剪（状态名改为 clamped_to_four_level_limit）而不是拒绝加载。
  2. EptPageTables.cpp:568 构建期对完全不含 RAM 的 1GB 区域只留 not-present 的 PDPTE（PML4 与 PDPT 结构本身仍按覆盖范围建满）；否则 MAXPHYADDR=46 需要 65536 个 PD 页（约 256MB/每 vCPU），既耗尽非分页池又拖慢加载。
  3. EptPageTables.cpp:708 新增 ept::ensure_mmio_mapping()：EPT violation 首次访问时补映射，支持 1GB EPT 大页（IA32_VMX_EPT_VPID_CAP[17]）的 CPU 直接填一个 1GB UC 条目，否则用 EptPageTables.cpp:659 publish_mmio_2mb_mapping() 从构建期预分配的 PD 页池取页填 512 个 2MB UC 条目（EPT_MMIO_PD_POOL_PAGE_COUNT=128）；VmExitEpt.cpp:162 在 unhandled 计数与 64 次限次保护之前调用，成功后 INVEPT 并直接返回（EPT violation 语义就是来宾重试该指令）。
  4. GlobalsEptPageTypes.h:83 增加 PDPTE 的 1GB 大页视图（地址字段 bit30 起，PFN = 1GB 区域索引）；EptAddressing.cpp:28、MtrrMemoryTypes.cpp:125/174、HostTables.cpp:779 按 pml2_pages[i] == nullptr 跳过 PDPTE 大页区域，并把按需映射 PD 池登记进 HOST_CR3（否则根模式首次写池页会因宿主页表缺页而崩）。
- 关键约束：ensure_mmio_mapping()/publish_mmio_2mb_mapping() 运行在 VM-exit（VMX root + 宿主私有栈）中，只做纯内存写入：不写文件日志（outFile 最终会 ZwCreateFile，在私有栈上做文件 IO 会触发 FAST_FAIL_INCORRECT_STACK）、不再调用任何会阻塞的分配接口。运行期统计只累加 ept_state 计数器，由 EptPageTables.cpp:252 在 cleanup_page_table() 的安全上下文输出。
- 验证：
  - VT_Driver Release x64 全量重建（WDKContentRoot=D:\Windows Kits\10）：0 error、0 warning，产出并签名 VT_Driver.sys。
  - 离线复算（用 GlobalsMacros.h 的真实掩码）：GPA 0x3FFAF000C60 → PML4 索引 7、PML3 索引 510、PML2 索引 376、flat_pdpte=4094、所在 1GB 区域基址 0x3FF80000000。修复前无论内存多大覆盖都是 512 个 1GB 区域（512GB），该 GPA 必然超出范围；修复后 MAXPHYADDR=42/46/48 位分别覆盖 4TB/64TB/256TB，该 GPA 均在范围内且落入“非 RAM → 首次访问按需映射”分支。
  - 上机核对点（VTLoad.log）：先出现 “[信息][EPT覆盖范围] 全平台MAXPHYADDR最大值=..位”（逐核扫描 CPUID.80000008H 的汇总）；随后每个核各出现一组带 CPU 前缀的行，例如 “[信息][CPU0][EPT能力] IA32_VMX_EPT_VPID_CAP=0x... 2MB大页=支持 1GB大页=支持”、“[信息][CPU0][EPT覆盖范围] 全平台MAXPHYADDR物理上界=0x.. 所需PDPT数量=..”、“[信息][CPU0][EPT页表] 映射规模：PDPT=65536 PML4=128”（46 位机；不再是 512/1）、“[信息][CPU0][EPT页表] 页表构成：含RAM的1GB区域=.. 非RAM按需映射区域=..”；24 个逻辑核应各有一套完整行，且各核的 1GB 大页能力按本核 MSR 取值；访问高地址 MMIO 后不再出现 Arg1=7 蓝屏，清理/卸载时可见 “[信息][EPT按需映射] 运行期统计：已补映射区域=.. 失败次数=0”。

### B17 每核 EPT 由“引导核统一构建”改为“在所属逻辑处理器上构建”，覆盖范围改用全平台最大 MAXPHYADDR

- 问题（混合架构隐患，12 代及以后 P-core + E-core）：VT_Driver/Vmm/VmmContext.cpp 的 allocate_vmm_context() 在原实现里一次循环调用 init_vcpu() 构建全部 vCPU 的 EPT，而这段代码运行在驱动程序所在的那个逻辑处理器上。IA32_VMX_EPT_VPID_CAP（1GB EPT 大页支持位 bit17）、CPUID.80000008H.MAXPHYADDR 与 MTRR 都是每逻辑处理器状态：在 P-core 上读出的能力被用来给 E-core 建表时，如果两者能力不一致（例如某核不支持 1GB EPT 大页），该核的 PDPTE 会带上 PS=1，硬件判 EPT misconfiguration（VM-exit reason 49，qualification 恒为 0），表现为加载即 0x00020001 Arg1=8。
- 修法：
  1. Vmm/VmmVcpu.cpp：init_vcpu() 只保留位图、__ept_state、宿主 GDT/IDT/TSS 等资源分配；新增 init_vcpu_ept()，把 ept::initialize() 放到该核自己的逻辑处理器上执行（打印 [CPU%u] 便于逐核核对）。
  2. Vmm/VmmProcessor.cpp：init_logical_processor2() 开头（KeSetSystemGroupAffinityThread 之后、VMXON 之前）调用 init_vcpu_ept()；失败直接返回 false，由 vmm_init() 现有的回滚路径 vmoff(iter) + free_vmm_context() 收尾（free_vmm_context 对 ept_pointer == nullptr 的部分初始化状态是安全的）。
  3. Vmm/VmmContext.cpp：新增 query_platform_physical_address_limit()，逐核读 CPUID.80000008H 取全平台最大物理地址上界并缓存到 g_vmm_context.platform_max_physical_end；Vmm/VmmEntry.cpp 在 vmm_init() 开头（IRQL <= APC_LEVEL）调用一次，失败则 free_vmm_context() 后放弃加载。
  4. Ept/Build/EptPageTables.cpp：query_required_pdpt_count() 改用 g_vmm_context.platform_max_physical_end（不再在 EPT 构建函数里读本核 CPUID）；覆盖范围必须取平台最大值，因为 MMIO/BAR 是操作系统按全平台解码能力分配的，按单核较小值建表会让该核访问高地址 BAR 时命中未映射条目。
- 不改动项：MTRR 仍全局读一次（build_mtrr_map()）。MTRR 由固件/操作系统在各核上统一编程，Intel 同包内取值一致；HyperPlatform（EptInitializeMtrrEntries）与 HyperDbg（EptCheckFeatures 顶部读 IA32_MTRR_DEF_TYPE）也都是初始化时读一次。
- 验证：VT_Driver Release x64 全量重建：0 error、0 warning，产出并签名 VT_Driver.sys。
- 上机核对点：VTLoad.log 中每个 CPU 都有一组 [CPU%u] 前缀的 [EPT能力]/[EPT覆盖范围]/[EPT页表] 行；若某个核不支持 1GB EPT 大页，该核应打印 1GB大页=不支持 并走 “非RAM按需映射：粒度=2MB页” 分支，不再出现 Arg1=8 的 EPT misconfiguration。

### B18 EPT 权限重算路径未把 entry 归一到原页，删掉断点后被调试进程仍卡

- 现象（用户报告，分支 codex/vt-watch-lifecycle-adjustments-20260925，HEAD e0a132b）：附加调试（目标为 valorant，日志含 ACE-IDS64.dll）下在 0x141F02D60 下 INT3 断点，命中两次后禁用并删除断点，日志出现 EXCEPTION_SINGLE_STEP @ 0x141F02D6E（= 断点地址 + 0xE，一条 14 字节指令的步过收尾）；随后删除所有断点并恢复运行，被调试进程仍然卡住、调试器本身正常；点步进又能收到事件。
- 判据：删除动作确实执行到了驱动（日志有“断点已删除”，且 Driver/DriverGuestCommands.cpp:78 的 RemoveBreakpoint 认 ept_watch_deactivate 的 0 与 2 为成功），所以不是 docs/21 记录的“IsBPXEnabled 读不到 0xCC → DeleteBPX 跳过写回”那条旧根因。
- 根因：隐藏 INT3 用“原页 + 伪页”双映射实现，切页必须改 EPT 表项的 page_frame_number；但三处“重算权限”的代码只改 read/write/execute 三个权限位、不写 page_frame_number：
  1. VT_Driver/Ept/Watch/EptWatchPageInstall.cpp:26 apply_active_watch_permissions_locked()；
  2. VT_Driver/Ept/Watch/EptWatchQuery.cpp:46 RestoreEptPageProperties()；
  3. VT_Driver/Ept/Watch/EptWatchDeactivate.cpp:56 ept_watch_deactivate_internal() 的 else（非 INT3）分支在 has == 0 时。
  Globals/MtfPendingState.cpp:75 的 stale 页清理走的也是第 1 处。只要 entry 曾被 ept_handle_int3_execute() 的 has_active_watch 分支切到 changed_entry（伪页），上述任何一条都会把权限重算作用在伪页上：entry 从此长期停在伪页、伪页内容与原页分叉，删掉断点后 guest 仍在伪页上命中断点字节。
- 与 e0a132b 的关系：同一提交把执行型硬件断点（DR7.RWn=00）从静默丢弃改为桥接 VMCALL_WATCH_EXECUTES（Hook/DebugBreak/Core/DebugRegisterHelpers.cpp），而 x64dbg / CE 的硬件断点默认就是执行型，于是“同页同时存在 bpType==3 与 bpType!=3 watch”的概率上升，删除时更容易走 has != 0 或上面的 else 分支。
- 修法：三处都补上 entry_address->page_frame_number = hooked_page_info->pfn_of_hooked_page;，把“归一到原页”变成重算权限时的无条件动作；伪页只在 ept_handle_int3_execute() 的命中窗口内被指向。
- 附带修复 1（无进展自旋）：VmExitEpt.cpp 剩余 2 处 mark_pending_mtf_page() 失败分支（EPTO_HOOK_FUNCTION 执行违规 :99、普通 EXECUTE watch :123）由 goto handled_return 改为 goto unhandled_after_release，与 docs/21 §3.4 已修的 3 处收敛为同一处理。
- 附带修复 2（读写判据不对称）：Hook/HookCallSet/Hooks/HookManager/HiddenInt3WriteRemoval.cpp 的 exactRestore 原判据写死 size == 1，而 x64dbg / TitanEngine 的 DeleteBPX 写回 BreakPointSize == UE_MAX_BREAKPOINT_SIZE == 2 字节，导致删除时的 2 字节写回永远被判成“非精确恢复”、HandleHiddenInt3Write 把写入透传给真实 WriteProcessMemory。现与读侧共用 Hook/HookCallSet/Hooks/HookInternal.h 新增的 kBreakpointVerifyIoSize == 2。
- 验证：VT_Driver / Hook Release x64 全量重建 0 error / 0 warning；x64dbg 与 x32dbg 各下 10 个 INT3 断点 → 反复命中/继续 100 次 → 删除所有断点 → 目标应立即恢复执行；叠加执行型硬件断点（bph）后重跑同一步骤。
- 备注：本机 MSBuild 无法从命令行调用，需在 IDE 内重建后上机验证。

## 待确认与已记录但未改动

### C01 NtRemoveProcessDebug 的访问模式差异（Win11 与 Win10 不一致）

- 现象：DbgkSysWin10/DbgkApi/Api/RemoveProcessDebug.cpp 用 PreviousMode = KeGetPreviousMode() 传给 ObReferenceObjectByHandle；DbgkSysWin11 同文件直接传 KernelMode，且完全去掉了 PreviousMode。
- 影响：KernelMode 会跳过对象管理器的访问检查（请求的是 PROCESS_SET_PORT 与 DEBUG_PROCESS_ASSIGN）。若真实 Win11 内核用的是 PreviousMode，则本驱动的该钩子比系统更宽松。
- 旁证：同一 Win11 驱动内的 NtSetInformationDebugObject 仍然使用 PreviousMode，两处不一致；Win11 该文件应是移植时的手误。
- 未改原因：无法在本机取得 Win11 ntoskrnl 反汇编作为铁证；改为更严格会改变行为，需要作者确认。
- 建议：改为 PreviousMode，与 Win10 版本和系统语义一致。

### C02 其它已核对为设计如此的项

- vmx_fatal_error 走 KeBugCheckEx：fail-fast 设计，见 07 文档。
- Win11 DbgkUnInitialize 返回 VOID 且在关闭回调恢复失败时 KeBugCheckEx；Win10 版本返回 NTSTATUS。属两套驱动的有意差异。
- _MINIMAL 精简模式下 rdtsc、invlpg、mov_dr 等处理器不会被触发，属性能取舍。
