/* SPDX-License-Identifier: GPL-3.0-or-later
 * IPC soket sunucusu birim testi (root gerekmez; /tmp'de soket).
 * Bir istemci baglanip focus ve status mesaji gonderir; sunucunun odak
 * mesajini dogru ayristirdigini ve durum yanitini dondurdugunu dogrular.
 * Ayrica gecersiz adin ve PID 1'e odagin reddedildigini
 * ({"type":"error"} yaniti) dogrular.
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
static int  g_count = 0;     /* kac odak mesaji KABUL edildi */
static int  g_errors = 0;    /* istemcinin aldigi {"type":"error"} sayisi */

static void focus_cb(const char *name, pid_t pid)
{
    if (g_count++ == 0) {        /* ilk kabul edilen odagi sakla */
        snprintf(g_name, sizeof(g_name), "%s", name);
        g_pid = pid;
    }
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

    /* 1) gecerli: kendi PID'imiz.  2) gecersiz ad (yol gecisi).
     * 3) init (PID 1): root bile olsa her zaman reddedilmeli. */
    char f[128];
    snprintf(f, sizeof(f), "{\"type\":\"focus\",\"name\":\"firefox\",\"pid\":%d}\n",
             (int)getpid());
    const char *bad = "{\"type\":\"focus\",\"name\":\"../../evil\",\"pid\":1}\n";
    const char *foreign = "{\"type\":\"focus\",\"name\":\"init\",\"pid\":1}\n";
    const char *s = "{\"type\":\"status\"}\n";
    (void)!write(fd, f, strlen(f));
    (void)!write(fd, bad, strlen(bad));
    (void)!write(fd, foreign, strlen(foreign));
    (void)!write(fd, s, strlen(s));

    /* Yanitlar: 2 hata satiri + 1 durum satiri (sirayla). */
    char buf[1024] = {0};
    size_t len = 0;
    int lines = 0;
    while (lines < 3 && len < sizeof(buf) - 1) {
        ssize_t n = read(fd, buf + len, sizeof(buf) - 1 - len);
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; i++) if (buf[len + i] == '\n') lines++;
        len += (size_t)n;
    }
    printf("  istemci yanitlari:\n%s", buf);
    fflush(stdout);              /* cocuk _exit ile cikar; tamponu bosalt */
    close(fd);
    int errs = 0;
    for (const char *q = buf; (q = strstr(q, "\"type\":\"error\"")); q++) errs++;
    return errs;   /* cikis kodu = alinan hata sayisi */
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

    int st = 0;
    waitpid(client, &st, 0);
    g_errors = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    ipc_shutdown();

    int fails = 0;
    printf("== dogrulama ==\n");
    if (strcmp(g_name, "firefox") == 0) printf("  [GECTI] odak adi 'firefox'\n");
    else { printf("  [KALDI] odak adi '%s'\n", g_name); fails++; }
    if (g_pid == (long)client) printf("  [GECTI] odak pid = istemci pid (%ld)\n", g_pid);
    else { printf("  [KALDI] odak pid %ld (beklenen %d)\n", g_pid, (int)client); fails++; }

    /* PID <= 1 root icin bile her zaman reddedilir: yalnizca 1 odak kabul. */
    int want_count = 1, want_err = 2;
    if (g_count == want_count) printf("  [GECTI] kabul edilen odak sayisi %d\n", g_count);
    else { printf("  [KALDI] kabul edilen odak sayisi %d (beklenen %d)\n", g_count, want_count); fails++; }
    if (g_errors == want_err) printf("  [GECTI] reddedilen mesaj sayisi %d (gecersiz ad + PID 1)\n",
                                     g_errors);
    else { printf("  [KALDI] reddedilen mesaj sayisi %d (beklenen %d)\n", g_errors, want_err); fails++; }


    printf("\n%s\n", fails == 0 ? "TUM TESTLER GECTI" : "BAZI TESTLER KALDI");
    return fails ? 1 : 0;
}
