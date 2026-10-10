# 广播 + vmcall 逻辑完整差异：原版 UnrealVTDbg1 vs 当前 Intel-VT

## 一、文件结构差异

### 原版 UnrealVTDbg1

- Guest 侧广播（单文件）：
  - `DbgkSysWin10\Hvm\hypervisor_gateway.cpp`（与 Win11 完全一致）
  - `DbgkSysWin11\Hvm\hypervisor_gateway.cpp`（271 行）
- Hypervisor 侧分发（单文件）：
  - `VT_Driver\vmcall_handler.cpp`（246 行，单个 `switch`）

### 当前 Intel-VT

- Guest 侧广播（拆成 4 个文件）：
  - `DbgkSysWin10/11\Hvm\HypervisorGatewayApi.cpp`（241 行，对外 API）
  - `DbgkSysWin10/11\Hvm\HypervisorGatewayBroadcast.cpp`（255 行，DPC 回调）
  - `DbgkSysWin10/11\Hvm\HypervisorGatewayBroadcastSupport.cpp`（76 行，SafeVmCall 等支撑）
  - `DbgkSysWin10/11\Hvm\HypervisorGatewayInternal.h`（72 行，参数结构 + 声明）
- Hypervisor 侧分发（拆成 2 个文件 + 1 个策略头）：
  - `VT_Driver\VmCall\vmcall_handler.cpp`（87 行，入口 + 参数解析）
  - `VT_Driver\VmCall\VmcallDispatch.cpp`（354 行，前置校验 + Execute 分发）
  - `Common\Ring0\Hvm\VmcallDispatchPolicy.h`（`IsEptMutation` 策略）

## 二、Guest 侧广播差异

### 1. VMCALL 包装

- 原版：直接 `__vm_call` / `__vm_call_ex`。
- 当前：`SafeVmCall` / `SafeVmCallEx`，用 `__try/__except` 包一层，异常时返回 `false`。

### 2. 参数结构

- 原版：
  - `HookFunctionArgs`（5 字段：target/proxy/origin/cr3/statuses）
  - `UnHookFunctionArgs`（4 字段）
  - `vmcall()` 复用 `HookFunctionArgs::target_address` 传 `vmcallinfo`（字段双关）。
- 当前：
  - `HookFunctionArgs`（9 字段，新增 `transaction_status` / `committed_processors` / `committed_processor_count` / `rollback_statuses`）
  - `SoftwareBreakpointBufferArgs`（断点缓冲专用，避免字段复用）
  - `VmcallBroadcastArgs`（vmcall 专用，独立字段）

### 3. hook_function（含失败回滚）

- 原版：`KeGenericCallDpc(broadcast_hook_function)`，每核 `__vm_call_ex(VMCALL_EPT_RIP_HOOK)`，成功 `statuses++`；返回 `statuses == N`。
- 当前：新增 `committed_processors` 逐核记录；部分核失败时用 `broadcast_rollback_hook_function` 对已 hook 的核回滚（最多重试 3 次），失败时 `*origin_function = nullptr`。

### 4. vmcall（核心差异）

- 原版签名：`bool vmcall(PVOID vmcallinfo)`。
  - 每核 `__vm_call(vmcallinfo->command, vmcallinfo, 0, 0)`，成功 `statuses++`。
  - 结果：**N 核 N 次 VMCALL**。
- 当前签名：`bool vmcall(PVOID vmcallinfo, unsigned __int64 parameter2, unsigned __int64 parameter3)`。
  - `broadcast_vmcall` 内：
    1. `TryReadVmcallCommand` 安全读取 `command`；
    2. `IsEptMutation(command)` 判断是否改 EPT；
    3. 非 EPT 改动：每核各自 `SafeVmCall(command)`（与原版一致，N 核 N 次）；
    4. EPT 改动：`KeSignalCallDpcSynchronize` 选出一个属主，属主单核 `SafeVmCall(command)` 一次，屏障后每核 `SafeVmCall(VMCALL_INVEPT_CONTEXT, all)` + `SafeVmCall(VMCALL_SYNC_CR3_EXIT_POLICY)`。
  - 结果：**N 核 1 次改表 + N 次 INVEPT(all) + N 次 CR3 同步**。

### 5. 软件断点缓冲

- 原版：`get_hide_software_breakpoint` / `set_hide_software_breakpoint`，复用 `HookFunctionArgs`（把 size 强转 `(void**)`）。
- 当前：拆成 `get_software_breakpoint_buffer` / `set_software_breakpoint_buffer`，独立 `SoftwareBreakpointBufferArgs`，且：
  - get 路径加 `transaction_owner` 屏障；
  - set 路径加屏障 + `INVEPT(all)` + `CR3 同步`。

### 6. 新增能力

- 当前新增 `synchronize_cr3_exit_policy()` + `broadcast_sync_cr3_exit_policy`（原版没有）。

### 7. 其它函数

- `vmoff` / `invept` / `ept_unhook`（2 个重载）/ `test_vmcall` / `hypervisor_visible`：
  - 原版与当前逻辑相同，仅 `__vm_call` 换成 `SafeVmCall`，`KeSignalCallDpcSynchronize` + `KeSignalCallDpcDone` 收拢成 `SignalDpcDone`。

## 三、Hypervisor 侧分发差异

### 1. 结构

- 原版：`vmexit_vmcall_handler(__vcpu* vcpu)` 内一个大 `switch`。
- 当前：拆成 `ReadRequest`（参数解析）+ `ArePreconditionsValid`（前置校验）+ 多个 `Execute*` + `Execute` 主 `switch`。

### 2. 前置校验（新增）

- 当前 `ArePreconditionsValid` 按命令做参数/EPT 状态校验；原版直接执行，无校验。

### 3. MTF 收敛（新增）

- 当前 `vmexit_vmcall_handler` 入口先处理 `vcpu->ept_state->mtf_pending`（调 `vmexit_monitor_trap_flag_handler`），原版没有。

### 4. 每核 vs 跨核（关键差异）

- 原版：所有命令都只操作当前 `*vcpu->ept_state`（每核改自己的表）。
- 当前：
  - hook / unhook（`VMCALL_EPT_RIP_HOOK` / `CC_HOOK` / `INT1_HOOK` / `HOOK_FUNCTION` / `UNHOOK_FUNCTION`）：仍只操作当前 `*vcpu->ept_state`。
  - `ExecuteReadEptFakePageMemory`：`for (i < processor_count) vcpu[i].ept_state` 跨核遍历。
  - `ExecuteSoftwareBreakpointBuffer`（set/get）：`for (i < processor_count) vcpu[i].ept_state` 跨核遍历。
  - `AreAllEptStatesValid`：`VMCALL_HIDE_SOFTWARE_BREAKPOINT` / `WATCH_*` 要求所有核 EPT 都有效。

### 5. 新增命令

- 当前新增：`VMCALL_PEEK_BREAKPOINT`、`VMCALL_CLEAR_BREAKPOINT_STATE`、`VMCALL_SYNC_CR3_EXIT_POLICY`、`VMCALL_SET_SOFTWARE_BREAKPOINT_BUFFER`、`VMCALL_GET_SOFTWARE_BREAKPOINT_BUFFER`。
- 原版只有 `VMCALL_READ_SOFTWARE_BREAKPOINT` / `VMCALL_HIDE_SOFTWARE_BREAKPOINT`。

### 6. 命令语义变化

- `VMCALL_INIT_OFFSET`：原版单参数 `InitOffset(p1)`；当前双参数 `InitOffset(p1, p2)`。
- `VMCALL_EPT_UNHOOK_FUNCTION`：当前多 `hv::clear_pending_mtf_metadata(vcpu)`。
- 入口边界校验：当前 `Execute` 开头校验 `request.reason > VMCALL_REASON_MAX`。

## 四、关键行为差异对照表

| 项 | 原版 UnrealVTDbg1 | 当前 Intel-VT |
|---|---|---|
| 广播方式 | `KeGenericCallDpc`，每核 `__vm_call` | `KeGenericCallDpc`，每核 `SafeVmCall` |
| 普通 vmcall | N 核 N 次 VMCALL | 非 EPT 命令：N 核 N 次 VMCALL |
| EPT 改动命令 | N 核 N 次改表（每核改自己） | 属主单核改 1 次 + 每核 INVEPT(all) + CR3 同步 |
| hypervisor 侧 hook | 只改当前核 `*vcpu->ept_state` | 只改当前核 `*vcpu->ept_state` |
| hypervisor 侧断点缓冲 | 无对应命令 | 跨核遍历 `vcpu[i].ept_state` |
| 异常保护 | 无 | `SafeVmCall` + `TryReadVmcallCommand` |
| 前置校验 | 无 | `ArePreconditionsValid` |
| hook 失败回滚 | 无 | `committed_processors` + rollback |

## 五、注意点

1. 当前仓库的 `VT_Driver\HypervisorGateway\hypervisor_gateway.cpp`（172 行）是一份旧式简单网关：直接 `__vm_call`、无 `IsEptMutation` 事务、`broadcast_hook_function` 用 `VMCALL_EPT_CC_HOOK`。它与 guest 侧新网关（`HypervisorGateway*.cpp`）并存，是否仍在被调用需要单独确认。

2. “属主单核改表”在 hypervisor 侧对 hook 命令仍是“只改属主自己的 `*vcpu->ept_state`”，其它核的 EPT 不会被这次 VMCALL 修改；与断点缓冲命令的“跨核遍历全部 `vcpu[i].ept_state`”不一致。这是原版到当前最值得再确认的设计点。
