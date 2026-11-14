# PROJECT KNOWLEDGE BASE — leopard-getty

登录管理器：首启检测 `xapi_UserOobeRequired()`，需要建号则 exec oobe（退出后 respawn）；否则
`xapi_UserLogin` 登录后 exec shell。

## 构建
    python3 gen_ninja.py && ninja build   # → out/leopard-getty.elf
    ninja pack                            # → out/leopard-getty-*.xbps

- 静态 ELF，依赖 `../XJ380-SDK/out/crt`（`include/` + `lib/x86_64/`）。
- 读 `/users/<用户名>/config` 的 `shell=`（读不到回退 `DEFAULT_SHELL`），不再硬编码 shell 路径。

## Console host

- 登录成功后不能直接 `execve(shell)`：先 `SXAH_MARK_IS_TERMINAL`，再 fork shell，
  父进程留在 `console_host.c` 的循环里。
- 循环职责：`SXAH_READ_OUTPUT_BUFFER` → `xapi_Printf`（owner 输出直写 console）→
  `SXAH_UNLOCK_OUTPUT_LOCK`；`SXAH_CHECK_INPUT_BUFFER` → `xapi_Getch` → 行编辑 →
  `SXAH_WRITE_INPUT_BUFFER`。
- 不新增内核原语；键盘字节由 `keyboard_dispatch_key_message()` 投递到 XTTTP sink。
- `is_shell` 是 XTTTP 终端标记，不是“shell 身份”；getty 退出后由 rc respawn 清除。
