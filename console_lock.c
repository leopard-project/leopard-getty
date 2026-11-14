#include "console_lock.h"

#include <xposix/socket.h>
#include <unistd.h>
#include <string.h>

#define CONSOLE_SOCKET_PATH "/run/console0.sock"

struct console_sockaddr_un {
        sa_family_t sun_family;
        char        sun_path[108];
};

static int g_lock_fd = -1;

static void fill_addr(struct console_sockaddr_un *addr)
{
        memset(addr, 0, sizeof(*addr));
        addr->sun_family = AF_UNIX;
        strncpy(addr->sun_path, CONSOLE_SOCKET_PATH, sizeof(addr->sun_path) - 1);
}

/* The kernel's bind/connect ABI currently caps addrlen at sizeof(sockaddr_in6),
 * so pass only the family + NUL-terminated path, not sizeof(sockaddr_un). */
static socklen_t console_addr_len(const struct console_sockaddr_un *addr)
{
        return (socklen_t)(2 + strlen(addr->sun_path) + 1);
}

int console_lock_acquire(void)
{
        struct console_sockaddr_un addr;
        int fd;

        if (g_lock_fd >= 0) return 0;

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return -1;

        fill_addr(&addr);
        if (bind(fd, (struct sockaddr *)&addr, console_addr_len(&addr)) != 0) {
                close(fd);
                return -1;
        }
        listen(fd, 8);
        g_lock_fd = fd;
        return 0;
}

int console_lock_is_busy(void)
{
        struct console_sockaddr_un addr;
        int fd;
        int busy;

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return 0;

        fill_addr(&addr);
        busy = connect(fd, (struct sockaddr *)&addr, console_addr_len(&addr)) == 0;
        close(fd);
        return busy;
}

void console_lock_release(void)
{
        if (g_lock_fd >= 0) {
                close(g_lock_fd);
                g_lock_fd = -1;
        }
}
