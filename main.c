#include "xtuiapi.h"
#include "libsys.h"   /* xapi_* / WSTR；libsys.h：enter_syscall */
#include "console_host.h"

/* XACT 2026v5 头文件删除了该宏，但内核 XAPI_INPUT 仍支持 flags=0x1（不回显）。 */
#ifndef XAPI_INPUT_NO_ECHO
#define XAPI_INPUT_NO_ECHO 0x1
#endif

#define DEFAULT_SHELL  "/apps/system/lpsh.elf"
#define OOBE_PATH      "/apps/leopard/leopard-oobe.elf"
#define USER_CAP       64
#define CONFIG_CAP     512
#define PATH_CAP       256

static void resolve_shell(const char *, char *, size_t);

static inline void read_line(char *buf, size_t cap, uint64_t flags)
{
        if (buf == NULL || cap == 0)
                return;
        buf[0] = '\0';
        enter_syscall(XAPI_INPUT, (uint64_t)buf, (uint64_t)cap, flags, 0, 0, 0);
}

static inline void wipe(char *buf, size_t n)
{
        while (n-- > 0)
                buf[n] = '\0';
}

int main(int argc, char *argv[], char *envp[])
{
        (void)argc;
        (void)argv;
        (void)envp;

        xapi_Printf("LeopardGetty: console login\n");

        for (;;) {
                /* 首次启动：优先 exec 独立 OOBE 程序 */
                if (xapi_UserOobeRequired() != 0) {
                        WSTR  oobe_argv[2]  = { (WSTR)OOBE_PATH, NULL };
                        WSTR  oobe_envp[1]  = { NULL };

                        xapi_Execve((WSTR)OOBE_PATH, oobe_argv, oobe_envp);   /* 成功则不返回 */
                        xapi_Printf("[Failed] start leopard-oobe\n");
                        xapi_Sleep(2000);
                        continue;
                }

                /* 登录：成功才继续 */
                char  username[USER_CAP];
                char  password[USER_CAP];
                int64_t result;
                do {
                        xapi_Printf("Username: ");
                        read_line(username, sizeof(username), 0);
                        xapi_Printf("Password: ");
                        read_line(password, sizeof(password), XAPI_INPUT_NO_ECHO);

                        /* 官方 SDK 的 xapi_UserLogin 走 xauth/mbedtls；Leopard 内核实现的是
		 * XAPI_USER_LOGIN 原语，直接调用 syscall 保持行为一致。 */
		result = (int64_t)enter_syscall(XAPI_USER_LOGIN, (uint64_t)username,
		                                (uint64_t)password, 0, 0, 0, 0);
                        wipe(password, sizeof(password));

                        if (!result) break;
                        /* -11：内核"尝试次数过多"返回码 */
                        if (result == -11) {
                                xapi_Printf("Too many attempts;\nwait and retry.\n");
                                xapi_Sleep(5000);
                                continue;
                        }
                        xapi_Printf("Login failed.\n");
                } while (result != 0);

                /* 根据配置的 shell 变量进入会话：exec 成功则不返回 */
                char shell[PATH_CAP];
                resolve_shell(username, shell, sizeof(shell));

                WSTR envp[1] = { NULL };
                if (getty_run_session(shell, envp) != 0) {
                        xapi_Exit(1);
                }
                xapi_Exit(0);
        }
}

static void resolve_shell(const char *username, char *out, size_t out_cap)
{
        /* 从 /users/<用户名>/config 读取 shell= 的值；读不到则回退 DEFAULT_SHELL。 */
        strncpy(out, DEFAULT_SHELL, out_cap - 1);
        out[out_cap - 1] = '\0';

        char config_path[PATH_CAP];
        snprintf(config_path, sizeof(config_path), "/users/%s/config", username);

        char buf[CONFIG_CAP];
        memset(buf, 0, sizeof(buf));
        int64_t n = (int64_t)xapi_ReadFile((WSTR)config_path, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
                return;

        char *line = buf;
        while (*line != '\0') {
                char *eol = strchr(line, '\n');
                if (eol != NULL)
                        *eol = '\0';

                if (strncmp(line, "shell=", 6) == 0) {
                        char *val = line + 6;
                        size_t len = strlen(val);
                        while (len > 0 && (val[len - 1] == '\r' || val[len - 1] == ' ' || val[len - 1] == '\t'))
                                val[--len] = '\0';
                        if (len > 0) {
                                strncpy(out, val, out_cap - 1);
                                out[out_cap - 1] = '\0';
                        }
                        return;
                }

                if (eol == NULL)
                        break;
                line = eol + 1;
        }
}
