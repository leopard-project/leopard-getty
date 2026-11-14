/*
 * console_host.c — leaky getty as a minimal Win32K-style console host.
 *
 * Login happens in main.c on the direct console.  After login:
 *   - mark this process group as the terminal (XTTTP sink + output buffer);
 *   - fork/exec the shell;
 *   - loop: drain shell output to the console, feed keyboard input to the
 *     shell's line buffer, and reap the shell.
 *
 * getty deliberately exits when the session ends so leopard-rc respawns a
 * fresh, unmarked getty.  Changing the terminal flag back is not needed.
 */
#include "console_host.h"

#define LINE_CAP 256

static void console_write(const char *text)
{
        if (text == NULL || text[0] == '\0') return;
        /* getty is the terminal owner; p_xapi_output_kernel renders owner
         * output directly to the console instead of its xtttp buffer. */
        xapi_Printf("%s", text);
}

static void feed_line(const char *line, size_t length)
{
        char buffer[LINE_CAP + 2];

        if (length > LINE_CAP) length = LINE_CAP;
        memcpy(buffer, line, length);
        buffer[length] = '\n';
        buffer[length + 1] = '\0';
        enter_syscall(SXAH_WRITE_INPUT_BUFFER, (uint64_t)buffer, 0, 0, 0, 0, 0);
}

int getty_mark_terminal(void)
{
        return (int)(int64_t)enter_syscall(SXAH_MARK_IS_TERMINAL, 0, 0, 0, 0, 0, 0);
}

int getty_read_line(const char *prompt, char *out, size_t cap, int no_echo)
{
        size_t length = 0;

        if (out == NULL || cap == 0) return -1;
        out[0] = '\0';
        if (prompt != NULL) console_write(prompt);

        for (;;) {
                char ch = xapi_Getch();

                if (ch == '\n' || ch == '\r') {
                        if (!no_echo) console_write("\n");
                        out[length] = '\0';
                        return 0;
                }
                if (ch == '\b' || ch == 127) {
                        if (length > 0) {
                                length--;
                                if (!no_echo) console_write("\b \b");
                        }
                        continue;
                }
                if (ch >= 32 && ch < 127 && length < cap - 1) {
                        out[length++] = ch;
                        if (!no_echo) {
                                char echo[2] = { ch, '\0' };
                                console_write(echo);
                        }
                }
        }
}

int getty_run_session(const char *shell, WSTR envp[])
{
        WSTR argv[2] = { (WSTR)shell, NULL };
        int64_t pid;
        char line[LINE_CAP];
        size_t length = 0;
        const int no_hang = 1;

        pid = (int64_t)xapi_Fork();
        if (pid < 0) return -1;

        if (pid == 0) {
                xapi_Execve((WSTR)shell, argv, envp);
                xapi_Printf("getty: cannot start shell\n");
                xapi_Exit(127);
        }

        for (;;) {
                int status = 0;
                int64_t reaped = (int64_t)enter_syscall(SYS_WAIT4, (uint64_t)pid,
                                                        (uint64_t)&status, (uint64_t)no_hang,
                                                        0, 0, 0);
                if (reaped == pid || reaped < 0) break;

                /* 1. shell/app output -> console */
                char output[1024];
                int64_t read_rc = (int64_t)enter_syscall(SXAH_READ_OUTPUT_BUFFER,
                                                          (uint64_t)output,
                                                          sizeof(output) - 1,
                                                          0, 0, 0, 0);
                if (read_rc == 0) {
                        output[sizeof(output) - 1] = '\0';
                        if (output[0] != '\0') console_write(output);
                        enter_syscall(SXAH_UNLOCK_OUTPUT_LOCK, 0, 0, 0, 0, 0, 0);
                }

                /* 2. keyboard -> shell line input (only while shell waits) */
                int64_t input_status = (int64_t)enter_syscall(SXAH_CHECK_INPUT_BUFFER,
                                                              0, 0, 0, 0, 0, 0);
                if ((input_status & XTTTP_INPUT_STATUS_WAITING) != 0) {
                        int no_echo = (input_status & XTTTP_INPUT_STATUS_NO_ECHO) != 0;

                        /* Blocking read: the shell is waiting for a line, so
                         * there is no pending output to drain until this key
                         * is handled (one key per loop keeps echo responsive). */
                        char ch = xapi_Getch();

                        if (ch == '\n' || ch == '\r') {
                                if (!no_echo) console_write("\n");
                                feed_line(line, length);
                                length = 0;
                        } else if (ch == '\b' || ch == 127) {
                                if (length > 0) {
                                        length--;
                                        if (!no_echo) console_write("\b \b");
                                }
                        } else if (ch >= 32 && ch < 127) {
                                if (length < sizeof(line) - 1) {
                                        line[length++] = ch;
                                        if (!no_echo) {
                                                char echo[2] = { ch, '\0' };
                                                console_write(echo);
                                        }
                                }
                        }
                } else {
                        /* Shell/app switched to raw input (curses): leave the
                         * line buffer alone, the app reads the sink directly. */
                        length = 0;
                }

                xapi_Sleep(1);
        }

        return 0;
}
