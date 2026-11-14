# PROJECT KNOWLEDGE BASE — leopard-getty

登录管理器：首启检测 `xapi_UserOobeRequired()`，需要建号则 exec oobe（退出后 respawn）；否则
`xapi_UserLogin` 登录后 exec shell。

## 构建
    python3 gen_ninja.py && ninja build   # → out/leopard-getty.elf
    ninja pack                            # → out/leopard-getty-*.xbps

- 静态 ELF，依赖 `../XJ380-SDK/out/crt`（`include/` + `lib/x86_64/`）。
- 读 `/users/<用户名>/config` 的 `shell=`（读不到回退 `DEFAULT_SHELL`），不再硬编码 shell 路径。

## Logon only

- getty 只负责 OOBE/认证/读取 `shell=`，不拥有 console。
- 登录成功后 `execve("/apps/leopard/leopard-conhost.elf", {shell}, NULL)`。
- `SXAH_MARK_IS_TERMINAL`、XTTTP 输入输出循环都在 `leopard-conhost`。
- 不要把 console host 逻辑重新塞回 getty。
