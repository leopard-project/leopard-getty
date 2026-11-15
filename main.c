/*
 * leopard-getty — PTY login service.
 *
 * Runs as a child of leopard-conhost on the PTY slave (fd 0/1/2).  It no
 * longer uses XAPI_INPUT/XTTTP: line editing and echo are done by the PTY
 * line discipline, and password no-echo is a termios ECHO toggle.  After
 * authentication it execs the user's shell in the same PTY session.
 */
#include "xtuiapi.h"
#include "libsys.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEFAULT_SHELL "/apps/system/lpsh.elf"
#define OOBE_PATH     "/apps/leopard/leopard-oobe.elf"
#define USER_CAP      64
#define CONFIG_CAP    512
#define PATH_CAP      256

#ifndef TCGETS
#define TCGETS 0x5401
#endif
#ifndef TCSETS
#define TCSETS 0x5402
#endif
#ifndef ECHO
#define ECHO 0000010
#endif

/* Matches OpenXSKernel/include/tty.h: struct termios. */
struct getty_termios {
        uint32_t c_iflag;
        uint32_t c_oflag;
        uint32_t c_cflag;
        uint32_t c_lflag;
        uint8_t  c_line;
        uint8_t  c_cc[20];
};

static void out(const char *text)
{
        if (text != NULL && text[0] != '\0')
                (void)write(1, text, strlen(text));
}

static void outf(const char *format, ...)
{
        char    buffer[256];
        va_list args;
        int     length;

        va_start(args, format);
        length = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (length > 0) {
                if ((size_t)length > sizeof(buffer) - 1) length = (int)sizeof(buffer) - 1;
                (void)write(1, buffer, (size_t)length);
        }
}

static int read_line(char *buffer, size_t capacity, int no_echo)
{
        struct getty_termios saved;
        struct getty_termios term;
        int    restore = 0;
        size_t length = 0;

        if (buffer == NULL || capacity == 0) return -1;
        buffer[0] = '\0';

        if (no_echo && ioctl(0, TCGETS, &term) == 0) {
                saved = term;
                term.c_lflag &= ~((uint32_t)ECHO);
                if (ioctl(0, TCSETS, &term) == 0) restore = 1;
        }

        for (;;) {
                char    ch;
                ssize_t got = read(0, &ch, 1);
                if (got < 0) {
                        if (errno == EINTR) continue;
                        break;
                }
                if (got == 0) break;
                if (ch == '\n' || ch == '\r') break;
                if (ch == '\b' || ch == 0x7f) {
                        if (length > 0) length--;
                        continue;
                }
                if (length + 1 < capacity) buffer[length++] = ch;
        }
        buffer[length] = '\0';

        if (restore) (void)ioctl(0, TCSETS, &saved);
        return (int)length;
}

static void wipe(char *buffer, size_t length)
{
        while (length-- > 0) buffer[length] = '\0';
}

static void resolve_shell(const char *username, char *out_path, size_t out_cap)
{
        strncpy(out_path, DEFAULT_SHELL, out_cap - 1);
        out_path[out_cap - 1] = '\0';

        char config_path[PATH_CAP];
        snprintf(config_path, sizeof(config_path), "/users/%s/config", username);

        char buf[CONFIG_CAP];
        memset(buf, 0, sizeof(buf));
        int64_t n = (int64_t)xapi_ReadFile((WSTR)config_path, buf, sizeof(buf) - 1, 0);
        if (n <= 0) return;

        char *line = buf;
        while (*line != '\0') {
                char *eol = strchr(line, '\n');
                if (eol != NULL) *eol = '\0';

                if (strncmp(line, "shell=", 6) == 0) {
                        char  *val = line + 6;
                        size_t len = strlen(val);
                        while (len > 0 && (val[len - 1] == '\r' || val[len - 1] == ' ' ||
                                           val[len - 1] == '\t'))
                                val[--len] = '\0';
                        if (len > 0) {
                                strncpy(out_path, val, out_cap - 1);
                                out_path[out_cap - 1] = '\0';
                        }
                        return;
                }
                if (eol == NULL) break;
                line = eol + 1;
        }
}

int main(int argc, char *argv[], char *envp[])
{
        (void)argc;
        (void)argv;

        out("LeopardGetty: console login\n");

        for (;;) {
                if (xapi_UserOobeRequired() != 0) {
                        WSTR oobe_argv[2] = { (WSTR)OOBE_PATH, NULL };
                        xapi_Execve((WSTR)OOBE_PATH, oobe_argv, (WSTR *)envp);
                        out("[Failed] start leopard-oobe\n");
                        xapi_Sleep(2000);
                        continue;
                }

                char    username[USER_CAP];
                char    password[USER_CAP];
                int64_t result;

                do {
                        out("Username: ");
                        read_line(username, sizeof(username), 0);

                        out("Password: ");
                        read_line(password, sizeof(password), 1);
                        out("\n");

                        result = (int64_t)enter_syscall(XAPI_USER_LOGIN,
                                                        (uint64_t)username,
                                                        (uint64_t)password,
                                                        0, 0, 0, 0);
                        wipe(password, sizeof(password));

                        if (result == 0) break;
                        if (result == -11) {
                                out("Too many attempts;\nwait and retry.\n");
                                xapi_Sleep(5000);
                                continue;
                        }
                        out("Login failed.\n");
                } while (result != 0);

                char shell[PATH_CAP];
                resolve_shell(username, shell, sizeof(shell));

                WSTR shell_argv[2] = { (WSTR)shell, NULL };
                xapi_Execve((WSTR)shell, shell_argv, (WSTR *)envp);

                outf("LeopardGetty: cannot start %s\n", shell);
                xapi_Sleep(2000);
                return 1;
        }
}
