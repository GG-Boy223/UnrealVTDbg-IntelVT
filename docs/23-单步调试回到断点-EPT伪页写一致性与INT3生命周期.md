# 23 单步调试「走几步又回到断点」—— EPT 伪页写一致性与 INT3 生命周期收尾

> 触发场景（用户报告）：下断点调试之后单步执行，走几步又回到断点；继续单步才会接着走。
> 用户已排除「程序路径本身就短 / 会自然绕回断点」这一解释。
> 前置：`docs/20`（CE/x64dbg/x32dbg 调试能力全量适配）、`docs/21`（断点删除后卡死）、`docs/22`（功能矩阵复核）。

---

## 0 一句话结论

> 隐藏 INT3 用「原页 + 伪页」双映射实现，而**伪页只是安装时刻的一份快照**。
> 原实现在隐藏态把原页的 write 位留成 1，于是 guest 对代码页的任何写入
> （调试器打补丁、目标自改代码、内核写进程内存）都**静默落在原页**，
> VMM 既不知道、也不会重新同步伪页；下一次执行该页时 `ept_handle_int3_execute`
> 换入的就是**过期快照**，guest 按旧指令流执行 —— 这正是「单步几步后跳回断点、
> 步进位置与源码不符」这类不按真实代码走的唯一驱动侧机理。
>
> **已修**：① 隐藏 INT3 页同时收紧 write 位（写必拦截 → 置 `fake_page_needs_full_sync`
> → MTF 收尾整页重新同步伪页）；② 页上最后一个 INT3 watch 消失时清掉 `changed_entry`，
> 消除「已经没有 INT3 的页仍走 INT3 分支」导致的无进展自旋；③ 一次性放行窗口给全权限、
> 收尾按 live watch 重算，保证「拦写」不会把页卡在 `write=0`。

---

## 1 调试器侧事实（源码取证）

调试器源码：`F:\001\VT\中间目录\TitanEngine`（x64dbg / x32dbg 使用的同一份引擎）。

### 1.1 单步（F7）只置 TF，**不**回写原字节

`TitanEngine.Debugger.Control.cpp:39-71 StepInto()`：

```cpp
ULONG_PTR ueCurrentPosition = GetContextData(UE_CIP);
MemoryReadSafe(hProcess, (void*)ueCurrentPosition, instr, 16, 0);   // 反汇编判前缀
if (strstr(DisassembledString, "PUSHF"))            StepOver(cb);
else if (POP SS || MOV SS)                          SetBPX(CIP + len, INT3 + UE_SINGLESHOOT, cb);
else { GetThreadContext(...); myDBGContext.EFlags |= UE_TRAP_FLAG; SetThreadContext(...); }   // ← 只置 TF
```

即：**单步不会去动断点字节**。它能正常工作的前提是「CIP 已经不在断点字节上」，
这一点由 §1.2 的断点命中流程保证。

### 1.2 断点命中：恢复原字节 + RIP 回退 + 自动 TF 单步 + 延迟重装

`TitanEngine.Debugger.DebugLoop.cpp:625-700`（`STATUS_BREAKPOINT` 分支，命中 `BreakPointBuffer`）：

```cpp
VirtualProtectEx(A, size, PAGE_EXECUTE_READWRITE, &OldProtect);
WriteProcessMemory(hProcess, A, &FoundBreakPoint.OriginalByte[0], BreakPointSize, ...);  // ① 写回原字节
GetThreadContext(...);
if (type != UE_SINGLESHOOT) { myDBGContext.EFlags |= UE_TRAP_FLAG; synchronizedStep = true; }  // ② 自动 TF
myDBGContext.Rip -= FoundBreakPoint.BreakPointSize;                                       // ③ RIP 回退到 A
SetThreadContext(...);                                                                    // ④ (RIP=A, TF=1)
myCustomBreakPoint();                                                                     // ⑤ 通知调试器
if (type != UE_SINGLESHOOT) { DisableBPX(A); ResetBPXAddressTo = A; ResetBPX = true; }     // ⑥ 待重装
```

重装在 `DebugLoop.cpp:772-790`，处理**下一个异常事件**时执行，且带一条护栏：

```cpp
if (ResetBPX) {
    if (ResetBPXAddressTo + ResetBPXSize != ExceptionAddress) {   // ResetBPXSize = BreakPointSize-1 = 0
        EnableBPX(ResetBPXAddressTo);                             // 例外地址仍是 A 时不重装
        ResetBPX = false;
    }
}
```

**推论（对本源最关键）**：断点命中后，guest 会**自动单步执行 A 处的真实指令**
（② ③ ④ 的作用），得到 `EXCEPTION_SINGLE_STEP @ A+len`；重装发生在该 #DB 之后，
此时 guest 已在 A+len，因此重装本身是安全的。

### 1.3 步过（F8）：`CIP+len` 处临时 INT3

`StepOver()`：`CALL / REP / PUSHF` → `SetBPX(CIP+len, INT3 + UE_SINGLESHOOT)`；其余退化为 `StepInto`。
临时断点命中时同样回退 RIP 到该地址、并 `DeleteBPX`，即「让那条指令真正执行一次」。

### 1.4 与本源 Hook 的关系

① 的 `WriteProcessMemory(A, OriginalByte)` 会被 `Hook/HookCallSet/Hooks/HookManager/HiddenInt3WriteRemoval.cpp`
识别成「恢复写入」→ `DeleteInt3BreakpointFromDriver` → **驱动侧整条 EPT watch 被删除**；
⑥ 的 `EnableBPX(A)` 再写 0xCC → 走 `HiddenInt3MemoryWrite.cpp` 的安装路径重新建立隐藏 INT3。
两侧的 `exactRestore` 判据（`kBreakpointVerifyIoSize == 2`）已由 `docs/21` §6.4 对齐。

---

## 2 本源侧时序（逐步）

设断点地址 `A`，页 `P`（`P` 上有 `bpType == 3` 的 watch）：

| 步骤 | EPT 表项 | 说明 |
| --- | --- | --- |
| 安装后（隐藏态） | 原页，`r=1 / w=1 / x=0` | `hide_cc_breakpoint` + `apply_active_watch_permissions_locked` |
| guest 执行 `P` | 执行违规 → `ept_handle_int3_execute` | `has_active_watch` → 换入 `changed_entry`（伪页，`r=0 / w=0 / x=1`），伪页 `A` 处为 `0xCC` |
| guest 执行 `A` | 伪页 | `#BP` → Dbgk → 调试器（§1.2 ①~⑥） |
| 调试器恢复原字节 | 伪页 → 原页 | 驱动删除 watch，表项归一到原页 |
| guest 自动 TF 单步 `A` 处真实指令 | 原页（已可执行） | `#DB @ A+len`；随后 §1.2 重装 watch |
| 之后每一步 | 在「原页 `x=0`」与「伪页 `x=1`」间振荡 | 伪页 `r/w=0`，页内任何数据访问都会再触发一次「换回原页 + MTF 收尾」 |

**关键点**：只要表项停在伪页，guest 执行的就是**伪页的快照**；而伪页只在
`!fake_page_synced || fake_page_needs_full_sync` 时重新整页同步
（`VmExitException.cpp` 的 MTF 收尾 / `sync_int3_fake_page_from_guest`）。

---

## 3 缺陷

### D1【根因】伪页写一致性缺口：隐藏态原页 `write = 1`

`EptWatchBreakpoints.cpp:144-147 prepare_int3_fake_page_entries()` 与
`EptWatchBreakpoints.cpp:353-357 hide_cc_breakpoint()` 都把原页建成 `read=1 / write=1 / execute=0`，
`apply_active_watch_permissions_locked()` 也只重算 read/write/execute、从不收紧 write。

后果：表项停在原页（**这是绝大多数时间的稳定态**）时，guest 对该页的写入
**不产生任何 VM-exit**，VMM 无从得知伪页已过期。下一次执行该页时换入的是过期快照，
guest 执行旧指令流 —— 旧指令流里的相对跳转/循环完全可以把执行流带回 `A`，
于是「走几步又回到断点」。

补充 1：`fake_page_needs_full_sync` 原先只在 `ept_handleWatchEvent` 的 **INT3 分支**里按
`write_access` 置位，覆盖不到「同页还有数据 watch 命中」与「命中缓存 ID」两条子路径。

补充 2：更关键的是，**该标记原先只在 MTF 收尾的 `is_int3` 分支被消费**，而伪页被用于执行的唯一入口是
`ept_handle_int3_execute()` 的 `has_active_watch` 分支 —— 只要有一次「置脏」但没走到 `is_int3` 收尾
（例如写入命中的是同页某条精确数据 watch），下一次执行就会把**过期快照**换进来执行。
因此 §4 第 5 项把「换入前必须先同步」变成该入口的硬前提。

### D2 `changed_entry` 陈旧：没有 INT3 的页仍走 INT3 分支 → 无进展自旋

`vmexit_ept_violation_handler()` 用 `changed_entry.page_frame_number != 0` 判断「本页是 INT3 页」。
但 `apply_active_watch_permissions_locked()` 从不回写这个字段：当同页最后一个 INT3 watch 被删除
（`has != 0`，页对象因其它 watch 存活而保留）时，`changed_entry` 仍指向旧伪页。
此后 guest 执行该页 → 走 `ept_handle_int3_execute()` → 页上已无 INT3 watch → `transitional_id == -1`
→ `return false` → 调用方走 `unhandled_after_release` → 表项权限没有任何变化 → guest 重试同一指令
→ 再次进入同一分支：**无进展自旋**，最终收敛为 `vmx_fatal_error`（0x101 / bugcheck）。

### D3【附带】一次性放行窗口的权限不对称

隐藏态收紧 write 后，「重试的访问」既可能是读也可能是写，而原实现只在窗口里放开
`original_entry.execute`（`VmExitEpt.cpp` / `EptWatchInt3Execute.cpp`），`write` 仍是 0
→ 写访问会在窗口里反复违规（同样的自旋）。另外 `ept_watch_deactivate_internal()` 的
`bpType == 3 && has == 0` 分支只把 `execute` 置 1，会把 `write=0` 留在已删除断点的页上。

---

## 4 修法（6 个文件，全部在 VT_Driver）

| # | 位置 | 改动 |
| --- | --- | --- |
| 1 | `Ept/Watch/EptWatchBreakpoints.cpp` `prepare_int3_fake_page_entries` / `hide_cc_breakpoint`（新页路径） | 隐藏态原页 `original_entry.write = 0`、新页 `entry_address->write = 0`。这两条路不经过 `apply_active_watch_permissions_locked`，必须同步收紧，否则新下的断点页在第一次 MTF 收尾前仍是「写不拦」（D1） |
| 2 | `Ept/Watch/EptWatchPageInstall.cpp` `apply_active_watch_permissions_locked` | 统计本页是否存在**有效** INT3 watch：有则 `entry_address->write = 0`；没有则清 `changed_entry.page_frame_number`（D1 + D2） |
| 3 | `VmExit/VmExitEpt.cpp` 读写分支 | 任何被拦截的写入（`changed_entry.page_frame_number != 0`）都置 `fake_page_needs_full_sync`，覆盖数据 watch / 缓存 ID 子路径（D1） |
| 4 | `VmExit/VmExitEpt.cpp` `bpType == 3` 放行窗口 | 窗口内给原页 `read=1 / write=1 / execute=1`（D3） |
| 5 | `Ept/Watch/EptWatchInt3Execute.cpp` | ① **换入伪页执行之前**，若伪页被标记为脏（`fake_page_needs_full_sync`）就先整页重新同步并重新写回本页所有 active INT3 字节 —— 保证「脏快照永远不会被执行」（D1）；② transitional 放行窗口给原页全权限（D3） |
| 6 | `Ept/Watch/EptWatchDeactivate.cpp` `bpType == 3 && has == 0` | 原页恢复 `read=1 / write=1 / execute=1`（D3） |
| 7 | `VmExit/VmExitException.cpp` MTF 收尾（`is_int3`） | 先统计本页是否仍有有效 INT3 watch：有 → 恢复隐藏态 `read=1 / write=0 / execute=0`；没有（窗口期间 watch 已消失）→ 改用 `apply_active_watch_permissions_locked()` 按 live watch 重算并归一原页，避免把 `x=0/w=0` 留在已无 INT3 的页上（D1 + D2 的收尾一致性） |

### 为什么是「拦写」而不是「每次执行前重新整页同步」

- 伪页只用于**执行**，其内容必须等于原页 + 各 INT3 偏移上的 `0xCC`。要保证这一点，
  要么每次换入前重读 4KB（每次执行违规都要读一次 guest 页，代价远大于一次写拦截），
  要么把「原页被写过」这个事件拦下来（一次写 = 一次 EPT violation + MTF，且写代码页本身极少见）。
  后者与 `original_entry.execute = 0` 同源：**影子页方案必须同时影子化写**。
- 与 KVM/开源实现的取向一致：KVM 的 EPT 影子/`mmu_shadow` 路径同样把「被监视页的写」变成
  fault 后在收尾时统一重建，而不是周期性重读。

---

## 5 验证清单（上机）

1. 全量重建 `VT_Driver` Release x64：0 error / 0 warning。
2. 单步主场景：x64dbg 与 CE 各下 1 个 INT3 断点 → 命中后连续 `F7`（单步）≥ 200 次 →
   步进位置必须与反汇编逐条一致，不得出现「跳回断点」。
3. 代码页写入场景（D1 的直接回归点）：在**断点所在页**上再手工修改若干字节
   （x64dbg 汇编窗口 `Space` 改字节 / 内存窗口改写）→ 继续单步 → guest 必须按**修改后**的
   指令流执行（这是原实现会跑旧快照的场景）。
4. 自改代码目标：任选一个会写自身代码页的目标（JIT / 壳），下断点后单步 + 运行交替 ≥ 100 次。
5. D2 回归点：同页同时下 `bp`（INT3）与 `bph`（执行型硬件断点）→ 删除 INT3 那个 →
   继续执行不得卡死 / 不得出现 `vmx_fatal_error`。
6. 删除路径回归（`docs/21` §5/§6.5 原清单）：删除全部断点后目标立即恢复执行，
   且内存中无残留 `0xCC`；`bpd` / `bpe` 各 10 次仍可命中。
7. 性能抽检：断点所在页执行 1000 次，观察是否出现异常的 VM-exit 风暴
   （写入被拦只会影响「写代码页」的路径，执行路径的 VM-exit 次数不应增加）。

---

## 6 未做 / 待确认

1. **未做**：`ept_handle_int3_execute()` 的 `transitional_id == -1` 分支仍是 `return false`。
   经 §4 第 1 项修好后，进入该函数的必要条件（`changed_entry != 0`）已等价于「本页存在有效 INT3 watch」，
   因此该分支理论上不可达，保留作为防御性返回；若上机仍出现该路径的 `unhandled_ept_violation_count`
   增长，再把它改成「按 live watch 重算权限 + 让 guest 重试」。
2. **待确认**：MTF 收尾与并发删除事务的竞态（MTF 已通过 `removing` 检查、删除事务随后才置位）——
   本轮的 `else` 分支已让这种情况收敛到「按 live watch 重算」，不再把 `x=0/w=0` 留在页上。
3. **待确认**：`RestoreEptPageProperties()`（`EptWatchQuery.cpp`）作为非 INT3 watch 的一次性放行窗口，
   仍写 `read=1 / write=1 / execute=1`，与 §4 第 3~5 项语义一致，本轮未改动。
4. 本机 MSBuild 无法从命令行调用，改动需在 IDE 内重建后上机验证。
