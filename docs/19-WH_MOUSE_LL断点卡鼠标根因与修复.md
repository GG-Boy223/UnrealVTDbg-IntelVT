# 19 - WH_MOUSE_LL 下断后鼠标卡住：根因与修复

> 场景：调试目标进程安装 `WH_MOUSE_LL` 低级鼠标钩子后，在目标线程下 INT3 软件断点，鼠标卡住。
> 结论：这是低级钩子回调线程被调试暂停后，输入链等待回调返回造成的，不是 INT3 本身把鼠标驱动停住了。

---

## 1. 根因

`SetWindowsHookExW(14, fn, nullptr, 0)` 对应的 HookId 是 `WH_MOUSE_LL`。
微软文档对低级鼠标钩子的关键语义是：

- 该钩子监控即将投递到线程输入队列的鼠标事件。
- 钩子过程在安装它的线程上下文中执行，系统通过向该线程发送消息来调用回调。
- 安装线程必须运行消息循环。
- 回调超过 `LowLevelHooksTimeout` 后，系统会跳过或移除该钩子；Windows 10 1709 之后默认上限为 1000 ms。

因此，调试器把安装钩子的目标线程停在断点时，该线程无法返回钩子回调。
低级输入链在此期间不能正常完成本次鼠标事件处理，表现就是鼠标卡顿。
这不是“调试器只有一个线程还是多个线程”的区别：等待发生在目标进程的安装线程上，
调试器自身有多少工作线程不会改变这个依赖关系。

微软文档入口：

- <https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc>
- <https://learn.microsoft.com/en-us/windows/win32/winmsg/hooks>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw>

---

## 2. HvFrontEnd 的公开处理方式

`caibarad/HvFrontEnd` 的 README 在 9.1 记录：

> 在调试相关程序时，程序会下 `WH_MOUSE_LL` 钩子，导致下断后鼠标卡住，已修复相关问题。

相关提交：

- <https://github.com/caibarad/HvFrontEnd/commit/0b09c6d>
- <https://github.com/caibarad/HvFrontEnd/commit/6113685>

该仓库没有公开驱动源码，9.1 的源码提交只更新了 README，随后 6113685 更新了
`HvFrontEnd.exe` 和 `HvGuardDbg.sys`。对更新后二进制的导入表、字符串和包装函数分析显示，
修复点是拦截 `win32kfull.sys` 中的 `NtUserSetWindowsHookEx`，并对
`HookId == WH_MOUSE_LL` 直接返回空句柄，而不是等钩子安装后再尝试恢复。

这比“断点命中时临时过滤某个线程”更可靠，因为它在输入链建立之前就消除了错误状态。

---

## 3. 本项目采用的最小修复

在 `DbgkSysWin10` 和 `DbgkSysWin11` 的 win32kfull Hook 链中增加：

```text
NtUserSetWindowsHookEx(
    Mod,
    UnsafeModuleName,
    ThreadId,
    HookId,
    HookProc,
    Ansi)
```

参数顺序与 ReactOS `win32k/user/ntuser/hook.c`、Wine `dlls/win32u/hook.c`
中的实现一致。

代理函数只做以下判断：

```cpp
if (HookId == WH_MOUSE_LL &&
    IsDebugTargetProcess(PsGetCurrentProcess(), &debugProcess))
{
    return NULL;
}
```

然后其他所有调用都原样转交给原始 `NtUserSetWindowsHookEx`。

边界如下：

- 只拦截 `HookId == 14`，即 `WH_MOUSE_LL`。
- 只拦截调试目标进程，调试器自身和其他进程不受影响。
- 不拦截 `WH_KEYBOARD_LL`、`WH_CBT`、`WH_GETMESSAGE` 等合法钩子。
- 不尝试“全量拦截” `NtUserSetWindowsHookEx`；全量拒绝会破坏窗口、消息、输入和第三方软件的合法行为。
- 只影响后续安装；如果目标在 Hook 生效前已经安装，需要重启目标或重新接管后再测试。

参考实现：

- <https://github.com/reactos/reactos/blob/master/win32ss/user/ntuser/hook.c>
- <https://github.com/wine-mirror/wine/blob/master/dlls/win32u/hook.c>

---

## 4. 调试线程模型说明

Windows 调试事件按进程和线程关联，不是“把整个调试器强行变成单线程”就能规避此问题。
低级鼠标钩子的回调发送到安装钩子的目标线程；断点暂停的也正是这条线程时，
输入子系统仍要等待该线程的回调完成或超时。

本项目保留多线程调试模型，修复放在 Hook 安装边界，而不是修改断点线程调度或全局关闭 INT3。

---

## 5. 验证

已完成：

- `DbgkSysWin10` Release x64：0 error / 0 warning
- `DbgkSysWin11` Release x64：0 error / 0 warning
- `VT_Driver` Release x64：0 error / 0 warning

硬件回归建议：

1. 加载修复后的驱动并接管目标进程。
2. 让目标调用 `SetWindowsHookExW(14, fn, nullptr, 0)`。
3. 确认该调用返回 `NULL`，目标没有安装 `WH_MOUSE_LL`。
4. 在目标其他线程下 INT3 断点，观察鼠标输入是否仍正常。
5. 对非调试目标进程和 `HookId != 14` 的调用回归，确认行为不变。
