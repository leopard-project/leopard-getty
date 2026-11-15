# PROJECT KNOWLEDGE BASE — leopard-getty

登录管理器：首启检测 `xapi_UserOobeRequired()`，需要建号则 exec oobe（退出后 respawn）；否则
`xapi_UserLogin` 登录后 exec shell。

## 构建
    python3 gen_ninja.py && ninja build   # → out/leopard-getty.elf
    ninja pack                            # → out/leopard-getty-*.xbps

- 静态 ELF，依赖 `../XJ380-SDK/out/crt`（`include/` + `lib/x86_64/`）。
- 读 `/users/<用户名>/config` 的 `shell=`（读不到回退 `DEFAULT_SHELL`），不再硬编码 shell 路径。

## Logon only

- 启动链：`rc → conhost → getty → shell`。getty 运行在 conhost 的 PTY slave 上。
- getty 只负责 OOBE/认证/读取 `shell=`，不拥有 console。
- 输入输出使用 fd 0/1/2；密码用 `TCGETS/TCSETS` 关闭 `ECHO`，不再用 `XAPI_INPUT`。
- 登录成功后 `execve(shell)`，shell 继承同一个 PTY。
- console/VT/fb 逻辑都在 `leopard-conhost`；不要把 console host 塞回 getty。
