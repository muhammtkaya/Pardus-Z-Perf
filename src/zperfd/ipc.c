/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ipc.h"
#include "log.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 8
#define CBUF_SIZE   2048

#define POLKIT_ACTION "org.pardus.zperf.manage"

typedef struct {
    int    fd;                 /* -1 = bos yuva */
    char   buf[CBUF_SIZE];     /* yarim satirlar icin biriktirme */
    size_t len;
    uid_t  uid;                /* SO_PEERCRED: baglanan surecin kimligi */
    pid_t  pid;
    int    auth;               /* ayar yetkisi: -1 bilinmiyor, 0 yok, 1 var */
} client_t;

static int           g_epfd = -1;
static int           g_listen = -1;
static char          g_path[108];
static ipc_focus_fn  g_on_focus;
static ipc_status_fn g_status;
static ipc_cmd_fn    g_on_cmd;
static client_t      g_clients[MAX_CLIENTS];
static long          g_trust_uid = -1;   /* yalnizca test: polkit'siz guvenilen uid */

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

/* --- yetki / dogrulama yardimcilari --- */

/* Uygulama adi cgroup dizin adina donusur (app-<ad>); yol gecisini ve JSON
 * enjeksiyonunu onlemek icin yalnizca [A-Za-z0-9._-] kabul edilir, '.' ile
 * baslayamaz. allow_space: profil adlari ("Akilli Tahta") icin. */
static int valid_name(const char *s, int allow_space)
{
    if (!s || !*s || *s == '.')
        return 0;
    for (const char *p = s; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (isalnum(ch) || ch == '_' || ch == '-' || ch == '.')
            continue;
        if (allow_space && ch == ' ')
            continue;
        return 0;
    }
    return 1;
}

/* /proc/<pid>/status icindeki gercek UID; hata halinde -1. */
static long proc_uid(pid_t pid)
{
    char path[64], buf[4096];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    if (util_read_str(path, buf, sizeof(buf)) < 0)
        return -1;
    const char *p = strstr(buf, "\nUid:");
    if (!p)
        return -1;
    return strtol(p + 5, NULL, 10);
}

/* /proc/<pid>/stat 22. alan (baslangic zamani); pkcheck PID yeniden
 * kullanimi yarisini onlemek icin bunu ister. Hata halinde 0. */
static unsigned long long proc_start_time(pid_t pid)
{
    char path[64], buf[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    if (util_read_str(path, buf, sizeof(buf)) < 0)
        return 0;
    char *p = strrchr(buf, ')');          /* comm parantez icerebilir */
    if (!p)
        return 0;
    /* Her adimda p, "field" numarali alandan onceki bosluga ilerler. */
    for (int field = 3; field <= 22 && p; field++)
        p = strchr(p + 1, ' ');
    return p ? strtoull(p + 1, NULL, 10) : 0;
}

/* Odak mesajindaki PID'i bu istemci yonetebilir mi? Root her seyi;
 * normal kullanici yalnizca KENDI sureclerini (baskasinin surecini
 * dondurtmak yetki yukseltmesi olurdu). */
static int focus_allowed(const client_t *c, pid_t target)
{
    if (target <= 1)
        return 0;
    if (c->uid == 0)
        return 1;
    long owner = proc_uid(target);
    return owner >= 0 && (uid_t)owner == c->uid;
}

/* Ayar komutlari (profil/ac-kapat/asla dondurma) icin yetki: root ise
 * dogrudan; degilse polkit (pkcheck, etkin yerel oturuma parolasiz).
 * Sonuc baglanti basina onbelleklenir. */
static int settings_allowed(client_t *c)
{
    if (c->uid == 0)
        return 1;
    /* YALNIZCA TEST: guvenilen uid polkit'siz gecer (bkz. ipc_set_trust_uid). */
    if (g_trust_uid >= 0 && (long)c->uid == g_trust_uid)
        return 1;
    if (c->auth >= 0)
        return c->auth;

    char proc[96];
    snprintf(proc, sizeof(proc), "%d,%llu,%u", (int)c->pid,
             proc_start_time(c->pid), (unsigned)c->uid);
    char *const argv[] = { "pkcheck", "--action-id", POLKIT_ACTION,
                           "--process", proc, NULL };
    int rc = util_run(argv);
    c->auth = (rc == 0);
    LOGI("polkit %s (uid=%u pid=%d): %s", POLKIT_ACTION, (unsigned)c->uid,
         (int)c->pid, c->auth ? "izin verildi" : "reddedildi");
    return c->auth;
}

static void reply(client_t *c, const char *line)
{
    char out[256];
    int n = snprintf(out, sizeof(out), "%s\n", line);
    if (n > 0) {
        ssize_t w = write(c->fd, out, (size_t)n < sizeof(out) ? (size_t)n : sizeof(out) - 1);
        (void)w;
    }
}

static void deny(client_t *c, const char *cmd, const char *why)
{
    char b[160];
    LOGW("IPC '%s' reddedildi (uid=%u pid=%d): %s", cmd, (unsigned)c->uid,
         (int)c->pid, why);
    snprintf(b, sizeof(b), "{\"type\":\"error\",\"cmd\":\"%s\",\"error\":\"%s\"}",
             cmd, why);
    reply(c, b);
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
        if (!json_str(line, "name", name, sizeof(name)) ||
            !json_int(line, "pid", &pid))
            return;
        if (!valid_name(name, 0)) { deny(c, "focus", "gecersiz-ad"); return; }
        if (!focus_allowed(c, (pid_t)pid)) { deny(c, "focus", "pid-yetkisi-yok"); return; }
        if (g_on_focus)
            g_on_focus(name, (pid_t)pid);
    } else if (strcmp(type, "status") == 0) {
        /* Uygulama listesi eklendigi icin tampon genis: ~APP_MAX giris. */
        char out[4096] = "{\"ok\":1}";
        if (g_status)
            g_status(out, sizeof(out));
        size_t n = strlen(out);
        if (n < sizeof(out) - 1) { out[n++] = '\n'; out[n] = '\0'; }
        ssize_t w = write(c->fd, out, strlen(out));
        (void)w;
    } else if (strcmp(type, "never_freeze") == 0) {
        char name[64] = {0};
        long on = 0;
        if (!json_str(line, "name", name, sizeof(name)) ||
            !json_int(line, "on", &on))
            return;
        if (!valid_name(name, 0)) { deny(c, "never_freeze", "gecersiz-ad"); return; }
        if (!settings_allowed(c)) { deny(c, "never_freeze", "yetki-yok"); return; }
        if (g_on_cmd)
            g_on_cmd("never_freeze", name, (int)on);
    } else if (strcmp(type, "profile") == 0) {
        char name[64] = {0};
        if (!json_str(line, "name", name, sizeof(name)))
            return;
        if (!valid_name(name, 1)) { deny(c, "profile", "gecersiz-ad"); return; }
        if (!settings_allowed(c)) { deny(c, "profile", "yetki-yok"); return; }
        if (g_on_cmd)
            g_on_cmd("profile", name, 0);
    } else if (strcmp(type, "enabled") == 0) {
        long on = 1;
        if (!json_int(line, "on", &on))
            return;
        if (!settings_allowed(c)) { deny(c, "enabled", "yetki-yok"); return; }
        if (g_on_cmd)
            g_on_cmd("enabled", "", (int)on);
    } else if (strcmp(type, "thaw") == 0) {
        char name[64] = {0};
        if (!json_str(line, "name", name, sizeof(name)))
            return;
        if (!valid_name(name, 0)) { deny(c, "thaw", "gecersiz-ad"); return; }
        if (!settings_allowed(c)) { deny(c, "thaw", "yetki-yok"); return; }
        if (g_on_cmd)
            g_on_cmd("thaw", name, 0);
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

        /* Istemci kimligi: odak PID sahipligi ve polkit yetkisi icin. */
        struct ucred cr;
        socklen_t crl = sizeof(cr);
        if (getsockopt(cfd, SOL_SOCKET, SO_PEERCRED, &cr, &crl) != 0) {
            LOGW("SO_PEERCRED okunamadi; baglanti reddedildi.");
            close(cfd);
            continue;
        }
        LOGD("IPC baglanti: uid=%d pid=%d", (int)cr.uid, (int)cr.pid);

        client_t *slot = client_slot(-1);
        if (!slot) {
            LOGW("IPC istemci limiti dolu; baglanti reddedildi.");
            close(cfd);
            continue;
        }
        slot->fd = cfd;
        slot->len = 0;
        slot->uid = cr.uid;
        slot->pid = cr.pid;
        slot->auth = -1;
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
    /* Soket herkese acik: odak ajani ve arayuz normal kullanici olarak
     * calisir. Yetki baglanti basina SO_PEERCRED + polkit ile denetlenir
     * (bkz. focus_allowed / settings_allowed). */
    if (chmod(path, 0666) < 0)
        LOGW("IPC soket izni ayarlanamadi: %s", strerror(errno));

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

void ipc_set_trust_uid(long uid)
{
    g_trust_uid = uid;
    if (uid >= 0)
        LOGW("GELISTIRME KAPISI: uid=%ld ayar komutlarini polkit'siz yapabilir "
             "(yalnizca test; uretimde ayarlamayin).", uid);
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

void ipc_broadcast(const char *line)
{
    if (!line || !*line)
        return;
    char msg[CBUF_SIZE];
    int n = snprintf(msg, sizeof(msg), "%s\n", line);
    if (n <= 0)
        return;
    size_t len = (size_t)n < sizeof(msg) ? (size_t)n : sizeof(msg) - 1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd < 0)
            continue;
        ssize_t w = write(g_clients[i].fd, msg, len);
        (void)w;  /* bloklamayan en iyi caba; hatada istemci sonraki okumada duser */
    }
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
