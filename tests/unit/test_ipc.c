/* SPDX-License-Identifier: GPL-3.0-or-later
 * IPC soket sunucusu birim testi (root gerekmez; /tmp'de soket).
 * Bir istemci baglanip focus ve status mesaji gonderir; sunucunun odak
 * mesajini dogru ayristirdigini ve durum yanitini dondurdugunu dogrular.
 *
 * Derleme:
 *   gcc -I src/zperfd tests/unit/test_ipc.c src/zperfd/ipc.c \
 *       src/zperfd/util.c src/zperfd/log.c -o /tmp/test_ipc
 */
#include "ipc.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define SOCK "/tmp/zperf_test.sock"

static char g_name[64] = {0};
static long g_pid = 0;

static void focus_cb(const char *name, pid_t pid)
{
    snprintf(g_name, sizeof(g_name), "%s", name);
    g_pid = pid;
}
static void status_cb(char *buf, size_t len)
{
    snprintf(buf, len, "{\"some\":1.50,\"full\":0.00,\"focus\":\"firefox\"}");
}

static int run_client(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof(a.sun_path), "%s", SOCK);
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { perror("connect"); return 1; }

    const char *f = "{\"type\":\"focus\",\"name\":\"firefox\",\"pid\":4321}\n";
    const char *s = "{\"type\":\"status\"}\n";
    (void)!write(fd, f, strlen(f));
    (void)!write(fd, s, strlen(s));

    char buf[256] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) printf("  istemci durum yaniti aldi: %s", buf);
    close(fd);
    return 0;
}

int main(void)
{
    log_set_level(LOG_WARN_);

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (ipc_init(epfd, SOCK, focus_cb, status_cb) != 0) {
        printf("ipc_init basarisiz\n");
        return 1;
    }

    pid_t client = fork();
    if (client == 0) { close(epfd); _exit(run_client()); }

    /* Sunucu dongusu: en fazla ~2 sn, odak alinana kadar olaylari isle. */
    for (int iter = 0; iter < 40 && g_name[0] == 0; iter++) {
        struct epoll_event evs[8];
        int n = epoll_wait(epfd, evs, 8, 50);
        for (int i = 0; i < n; i++)
            if (ipc_owns_fd(evs[i].data.fd))
                ipc_handle_fd(evs[i].data.fd);
    }
    /* Durum yanitinin yazilmasi icin birkac tur daha isle. */
    for (int iter = 0; iter < 10; iter++) {
        struct epoll_event evs[8];
        int n = epoll_wait(epfd, evs, 8, 50);
        for (int i = 0; i < n; i++)
            if (ipc_owns_fd(evs[i].data.fd))
                ipc_handle_fd(evs[i].data.fd);
    }

    waitpid(client, NULL, 0);
    ipc_shutdown();

    int fails = 0;
    printf("== dogrulama ==\n");
    if (strcmp(g_name, "firefox") == 0) printf("  [GECTI] odak adi 'firefox'\n");
    else { printf("  [KALDI] odak adi '%s'\n", g_name); fails++; }
    if (g_pid == 4321) printf("  [GECTI] odak pid 4321\n");
    else { printf("  [KALDI] odak pid %ld\n", g_pid); fails++; }

    printf("\n%s\n", fails == 0 ? "TUM TESTLER GECTI" : "BAZI TESTLER KALDI");
    return fails ? 1 : 0;
}
