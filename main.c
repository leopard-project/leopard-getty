#include "xtuiapi.h"   /* xapi_* / WSTR；libsys.h：enter_syscall / XAPI_INPUT_NO_ECHO */

#define SHELL          "/apps/system/xsh.elf"
#define OOBE_PATH      "/apps/leopard/leopard-oobe.elf"
#define USER_CAP       64

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

            result = (int64_t)xapi_UserLogin((WSTR)username, (WSTR)password);
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

        /* 进入会话：exec 成功则不返回 */
        WSTR argv[2] = { (WSTR)SHELL, NULL };
        WSTR envp[1] = { NULL };
        xapi_Execve((WSTR)SHELL, argv, envp);

        xapi_Printf("LeopardGetty: session start failed\n");
        xapi_Sleep(2000);
    }
}
