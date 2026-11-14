#pragma once

#include "xtuiapi.h"

/* Private XJ380 extension syscalls used by the console host. */
#define SXAH_MARK_IS_TERMINAL     128956723895689204ULL
#define SXAH_WRITE_INPUT_BUFFER   128956723895689205ULL
#define SXAH_READ_OUTPUT_BUFFER   128956723895689206ULL
#define SXAH_CHECK_INPUT_BUFFER   128956723895689207ULL
#define SXAH_UNLOCK_OUTPUT_LOCK   128956723895689208ULL

#define XTTTP_INPUT_STATUS_WAITING 0x1
#define XTTTP_INPUT_STATUS_NO_ECHO 0x2

/* Mark this process group as the XTTTP terminal and own its sink. */
int getty_mark_terminal(void);

/* Read one line from the XTTTP sink (getty is the terminal owner).  Used for
 * login so username/password never take the legacy raw-keyboard path. */
int getty_read_line(const char *prompt, char *out, size_t cap, int no_echo);

/* Login succeeded: run the shell and stay alive until it exits. */
int getty_run_session(const char *shell, WSTR envp[]);
