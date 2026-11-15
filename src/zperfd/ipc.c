/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ipc.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 8
#define CBUF_SIZE   2048

typedef struct {
    int    fd;                 /* -1 = bos yuva */
    char   buf[CBUF_SIZE];     /* yarim satirlar icin biriktirme */
    size_t len;
} client_t;

static int           g_epfd = -1;
static int           g_listen = -1;
static char          g_path[108];
static ipc_focus_fn  g_on_focus;
static ipc_status_fn g_status;
static ipc_cmd_fn    g_on_cmd;
static client_t      g_clients[MAX_CLIENTS];

/* --- kucuk JSON ayikla (tam ayristirici degil; bilinen alanlar icin) --- */

/* "key":"deger"  -> out. Basari 1, yoksa 0. */
static int json_str(const char *s, const char *key, char *out, size_t len)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(s, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    p++;
    while (*p == ' ') p++;
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < len - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return 1;
}

/* "key":123  -> *out. Basari 1. */
static int json_int(const char *s, const char *key, long *out)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(s, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    *out = strtol(p + 1, NULL, 10);
    return 1;
}

/* --- istemci yonetimi --- */

static client_t *client_slot(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g_clients[i].fd == fd)
            return &g_clients[i];
    return NULL;
}

static void client_close(client_t *c)
{
    if (c->fd >= 0) {
        epoll_ctl(g_epfd, EPOLL_CTL_DEL, c->fd, NULL);
        close(c->fd);
    }
    c->fd = -1;
    c->len = 0;
}

static void dispatch_line(client_t *c, char *line)
{
    if (!*line)
        return;

    char type[32] = {0};
    if (!json_str(line, "type", type, sizeof(type)))
        return;

    if (strcmp(type, "focus") == 0) {
        char name[64] = {0};
        long pid = 0;
        if (json_str(line, "name", name, sizeof(name)) &&
            json_int(line, "pid", &pid) && g_on_focus) {
            g_on_focus(name, (pid_t)pid);
        }
    } else if (strcmp(type, "status") == 0) {
        char out[512] = "{\"ok\":1}";
        if (g_status)
            g_status(out, sizeof(out));
        size_t n = strlen(out);
        if (n < sizeof(out) - 1) { out[n++] = '\n'; out[n] = '\0'; }
        ssize_t w = write(c->fd, out, strlen(out));
        (void)w;
    } else if (strcmp(type, "never_freeze") == 0) {
        char name[64] = {0};
        long on = 0;
        if (json_str(line, "name", name, sizeof(name)) &&
            json_int(line, "on", &on) && g_on_cmd) {
            g_on_cmd("never_freeze", name, (int)on);
        }
    }
}

/* Biriken tamponda tam satirlari isleyip kalani basa cek. */
static void process_buffer(client_t *c)
{
    size_t start = 0;
    for (size_t i = 0; i < c->len; i++) {
        if (c->buf[i] == '\n') {
            c->buf[i] = '\0';
            dispatch_line(c, c->buf + start);
            start = i + 1;
        }
    }
    if (start > 0) {
        memmove(c->buf, c->buf + start, c->len - start);
        c->len -= start;
    }
}

static void client_readable(client_t *c)
{
    for (;;) {
        if (c->len >= CBUF_SIZE - 1) {   /* asiri uzun satir: at */
            c->len = 0;
        }
        ssize_t n = read(c->fd, c->buf + c->len, CBUF_SIZE - 1 - c->len);
        if (n > 0) {
            c->len += (size_t)n;
            process_buffer(c);
            continue;
        }
        if (n == 0) {                    /* karsi taraf kapatti */
            client_close(c);
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;                      /* su an veri yok */
        if (errno == EINTR)
            continue;
        client_close(c);                 /* gercek hata */
        return;
    }
}

static void accept_clients(void)
{
    for (;;) {
        int cfd = accept4(g_listen, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (cfd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                LOGW("accept basarisiz: %s", strerror(errno));
            return;
        }

        /* Istemci kimligini al (gelistirmede bilgi; polkit sonraki asamada). */
        struct ucred cr;
        socklen_t crl = sizeof(cr);
        if (getsockopt(cfd, SOL_SOCKET, SO_PEERCRED, &cr, &crl) == 0)
            LOGD("IPC baglanti: uid=%d pid=%d", (int)cr.uid, (int)cr.pid);

        client_t *slot = client_slot(-1);
        if (!slot) {
            LOGW("IPC istemci limiti dolu; baglanti reddedildi.");
            close(cfd);
            continue;
        }
        slot->fd = cfd;
        slot->len = 0;
        struct epoll_event ev = { .events = EPOLLIN, .data = { .fd = cfd } };
        if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
            LOGW("istemci fd epoll'a eklenemedi: %s", strerror(errno));
            slot->fd = -1;
            close(cfd);
        }
    }
}

int ipc_init(int epfd, const char *path,
             ipc_focus_fn on_focus, ipc_status_fn status)
{
    g_epfd = epfd;
    g_on_focus = on_focus;
    g_status = status;
    snprintf(g_path, sizeof(g_path), "%s", path);
    for (int i = 0; i < MAX_CLIENTS; i++)
        g_clients[i].fd = -1;

    g_listen = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (g_listen < 0) {
        LOGE("IPC socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    unlink(path);  /* onceki kalinti */

    if (bind(g_listen, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOGE("IPC bind (%s): %s", path, strerror(errno));
        close(g_listen);
        g_listen = -1;
        return -1;
    }
    if (listen(g_listen, 8) < 0) {
        LOGE("IPC listen: %s", strerror(errno));
        close(g_listen);
        g_listen = -1;
        return -1;
    }

    struct epoll_event ev = { .events = EPOLLIN, .data = { .fd = g_listen } };
    if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, g_listen, &ev) < 0) {
        LOGE("IPC listen fd epoll'a eklenemedi: %s", strerror(errno));
        close(g_listen);
        g_listen = -1;
        return -1;
    }

    LOGI("IPC soketi hazir: %s", path);
    return 0;
}

void ipc_set_cmd_handler(ipc_cmd_fn fn)
{
    g_on_cmd = fn;
}

int ipc_owns_fd(int fd)
{
    if (fd == g_listen)
        return 1;
    return client_slot(fd) != NULL;
}

void ipc_handle_fd(int fd)
{
    if (fd == g_listen) {
        accept_clients();
        return;
    }
    client_t *c = client_slot(fd);
    if (c)
        client_readable(c);
}

void ipc_shutdown(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g_clients[i].fd >= 0)
            client_close(&g_clients[i]);
    if (g_listen >= 0) {
        epoll_ctl(g_epfd, EPOLL_CTL_DEL, g_listen, NULL);
        close(g_listen);
        g_listen = -1;
    }
    if (g_path[0])
        unlink(g_path);
}
