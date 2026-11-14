#pragma once

/* Userspace console ownership lock.
 *
 * A console host binds a Unix socket at /run/console0.sock for its lifetime.
 * A second getty/conhost probes the socket and refuses to start.  The kernel
 * only provides socket primitives; console policy stays in userspace. */
int  console_lock_acquire(void);
int  console_lock_is_busy(void);
void console_lock_release(void);
