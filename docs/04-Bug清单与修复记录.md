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
