# 24 F7 / 0x1E：EPT 伪页错位与注入解析栈覆盖修复

> 转储：`F:\MiniDmp\092826-6125-01.dmp`、`F:\MiniDmp\MEMORY.DMP`、
> `F:\MiniDmp\092826-5750-01.dmp`、`F:\MiniDmp\092826-6015-01.dmp`
> 目标：把 F7 和 0x1E 从“nt 的异常/Cookie 报错”还原到项目驱动的真实生命周期问题，避免把症状当成根因。

## 1 F7 现场是结果，不是根因

正确符号下的链是：

```
nt!KeBugCheckEx
nt!_report_gsfailure
nt!_GSHandlerCheckCommon
nt!_GSHandlerCheck
nt!RtlpExecuteHandlerForException
nt!RtlDispatchException
nt!KiDispatchException
nt!KiExceptionDispatch
nt!KiGeneralProtectionFault
```

这是异常处理路径在 `_GSHandlerCheckCommon` 检查栈 cookie 时失败。期望 cookie 与栈上 cookie 不一致说明执行流/栈已经被前面的错误破坏；不能把 `nt!_report_gsfailure` 当作 EPT 或 VMX 的根因。

## 2 伪页错位的直接证据

现场 trap frame：

```
rip = ffffd680bcfd6131
cs  = 0x10
rsp = ffff9c0f782d6d70
ss  = 0x18
```

`ffffd680bcfd6000` 所在页被导出为 `F:\MiniDmp\temp\f7-pool-page.bin`。逐字节比对结果：

1. 该页不是从目标页的 **页首** 复制出来的。
2. 该页的前 4096 字节与 ntoskrnl 内存镜像从 `0xA0B7DF` 开始的内容完全一致。
3. 后续连续页分别匹配 `0xA0C7DF`、`0xA0D7DF`、`0xA0E7DF`，即源地址始终保留了页内偏移 `0x7DF`。
4. 故障地址 `ffffd680bcfd6131` 对应伪页内偏移 `0x131`，该处字节为 `0F 22 C0`，即 `mov cr4, rax`。
5. 伪页内没有作为 Hook 入口写入的 `OF 01 C1` VMCALL，也没有该入口对应的隐藏 INT3 字节。

这说明当时 EPT 指向的执行页是“目标地址未页对齐后复制的 nt 代码副本”。执行流从错位的代码中间进入，寄存器状态被前一串错误指令改变，最终在 `mov cr4, rax` 触发 #GP；F7 只是 #GP 进入 nt 异常处理后，在 GS 栈 cookie 检查处爆出的二级症状。

## 2.1 `0x1E KMODE_EXCEPTION_NOT_HANDLED` 是同一根因的另一表现

重新读取另外两份转储：

```text
F:\MiniDmp\092826-5750-01.dmp
Arg1 = 0xFFFFFFFFC0000096  STATUS_ILLEGAL_INSTRUCTION
Arg2 = 0xFFFFC2818AA86014

F:\MiniDmp\092826-6015-01.dmp
Arg1 = 0xFFFFFFFFC0000096  STATUS_ILLEGAL_INSTRUCTION
Arg2 = 0xFFFFCD0C893183A3
```

这里的 `Arg2` 不是 ExceptionRecord 地址，而是异常发生地址。两份转储从
`Arg2` 导出的 512 字节具有同一前缀：

```text
0F 22 C0 41 8B 81 A0 0A 00 00 4D 8D B1 A8 0A 00 00 ...
```

与 `ntoskrnl` 内存镜像逐字节比对，二者都精确命中 RVA：

```text
ntoskrnl RVA 0xA0B910
```

而 F7 伪页的复制源是 RVA `0xA0B7DF`，故障 `rip` 位于伪页 `+0x131`：

```text
0xA0B7DF + 0x131 = 0xA0B910
```

因此三个蓝屏不是三个独立问题：

- `0xF7`：在错位伪页上执行 nt 代码，异常处理期间触发 GS cookie 检查失败。
- `0x1E`：在错位伪页上的同一 `mov cr4, rax` 地址发生非法指令/异常，最终升级为
  `KMODE_EXCEPTION_NOT_HANDLED`。

共同根因都是伪页整页复制源保留了 `0x7DF` 页内偏移，必须从目标页页首复制。

## 3 源码修复

### 3.1 伪页复制统一到页首

文件：

- `VT_Driver/Ept/EptHook.h`
- `VT_Driver/Ept/Hook/Instruction/EptHookInstruction.cpp`
- `VT_Driver/Ept/Hook/Lifecycle/EptHookVmcall.cpp`
- `VT_Driver/Ept/Hook/Lifecycle/EptHookCc.cpp`
- `VT_Driver/Ept/Hook/Lifecycle/EptHookInt1.cpp`
- `VT_Driver/Ept/Hook/Lifecycle/EptHookDefault.cpp`

新增 `initialize_fake_page_from_target()`，在函数内部完成：

```cpp
const unsigned __int64 target_page_base =
    reinterpret_cast<unsigned __int64>(target_function) & ~(kEptPageSize - 1);
RtlCopyMemory(hooked_page_info->fake_page_contents,
              reinterpret_cast<const void*>(target_page_base),
              kEptPageSize);
```

四个函数 Hook 建立路径不再各自写 `RtlCopyMemory`，也不再依赖调用点对 `PAGE_ALIGN` 的理解；伪页源地址与目标页页首只有一个实现点。

同一 `ept::page_align_down()` 也用于 `EptWatchBreakpoints.cpp`、
`EptWatchInt3Execute.cpp` 和 `VmExitException.cpp` 的 INT3 伪页整页同步，避免
函数 Hook 与隐藏 INT3 两条路径出现不同的页首定义。

### 3.2 跳板解码上限

文件：`VT_Driver/Ept/Hook/Instruction/EptHookInstruction.cpp`

`decode_hooked_instructions_size()` 现在同时拒绝：

- LDE 解码失败（0）；
- 单条指令超过 x64 的 15 字节上限；
- 覆盖指令加绝对跳转超过固定跳板缓冲区；
- 覆盖范围跨出 4KB 页。

这防止异常解码结果进入 `RtlCopyMemory(first_trampoline_address, target_function, hooked_instructions_size)`。

### 3.3 注入解析链固定栈缓冲覆盖

文件：

- `Common/Ring0/Inject/ApcInject/Core/ApiSetResolver.cpp`
- `Common/Ring0/Inject/ApcInject/Core/SxsResolver.cpp`
- `Common/Ring0/Inject/ApcInject/Core/StringUtilities.cpp`
- `Common/Ring0/Inject/ApcInject/Core/ImagePathResolver.cpp`
- `Common/Ring0/Inject/ApcInject/Core/KernelResolver.cpp`

改动点：

1. API Set Map 的 `NameLength`、`ValueLength` 不再直接 `memcpy` 到 `wchar_t[255]`。长度、奇数字节和终止符空间不足时直接拒绝该条解析项。
2. SxS 的 `name->Length` 不再直接复制到 `origBuf[0x100]`；复制前检查长度和 Unicode 对齐，复制后显式补 NUL。
3. `BBSafeInitString()` 按实际 `Length + sizeof(WCHAR)` 分配并按实际长度复制，拒绝 `Length > MaximumLength`、奇数长度和分配失败。
4. `BBSafeAllocateString()` 对奇数长度直接拒绝，避免后续 Unicode 追加越过 `MaximumLength`。
5. `ImagePathResolver.cpp` 用 `fullResolved.MaximumLength` 清零，避免固定 `0x400` 与实际分配长度不一致。
6. `GetKernelBase()` 对 `ExAllocatePoolWithTag` 失败不再继续 `RtlZeroMemory(NULL, bytes)`。

这些检查针对的是 PEB/注入请求中的长度字段和真实分配失败路径，不是无条件判空；它们直接阻止固定栈缓冲被外部长度覆盖。

## 4 验证

已完成：

- `VT_Driver` Release x64：构建通过。
- `DbgkSysWin10` Release x64：构建通过。
- `DbgkSysWin11` Release x64：构建通过。
- `UnrealDbg.sln` Release x64：构建通过（为避免本机证书库权限问题，验证构建使用 `SignMode=Off`）。
- `Hook` Release x86：构建通过，生成 `Release\Hook.dll`，供 x32dbg 使用。
- `VT_Driver` / `DbgkSysWin10` Code Analysis：完成，新增改动点没有新的栈覆盖警告；剩余告警为既有 SAL/遗留代码告警。

未完成：

- 真实 Win10 / Win11 上按“INT3 断点 → 命中 → 连续 F7 ≥ 200 次 → 删除断点”重新运行并确认无 F7。
- x64dbg / x32dbg / CE 三种前端各自回归硬件断点、执行断点和步出路径。
