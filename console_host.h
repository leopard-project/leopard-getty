#pragma once

#include "xtuiapi.h"

/* Private XJ380 extension syscalls used by the console host. */
#define SXAH_MARK_IS_TERMINAL     128956723895689204ULL
#define SXAH_WRITE_INPUT_BUFFER   128956723895689205ULL
#define SXAH_READ_OUTPUT_BUFFER   128956723895689206ULL
#define SXAH_CHECK_INPUT_BUFFER   128956723895689207ULL
#define SXAH_UNLOCK_OUTPUT_LOCK   128956723895689208ULL
#define SXAH_WRITE_CONSOLE        128956723895689214ULL
#define SXAH_GETCH_TRY            128956723895689215ULL
#define SXAH_GET_SCREEN_SIZE      128956723895689216ULL

#define XTTTP_INPUT_STATUS_WAITING 0x1
#define XTTTP_INPUT_STATUS_NO_ECHO 0x2

/* Login succeeded: become the console host, run the shell, and stay alive
 * until the shell exits.  Returns 0 if the session terminated normally. */
int getty_run_session(const char *shell, WSTR envp[]);
