/* Sorumlu: Furkan (sunucu), Melih (protokol tasarımı ve istemciler).
 * Altyapı (bağlantı kabulü, satırlara bölme, yanıt gönderme) hazırdır; komutların
 * içi TODO olarak bırakılmıştır. */
#include "ipc.h"
#include "auth.h"
#include "log.h"
#include "psi.h"
#include "util.h"
#include "zperfd.h"
#include "zram.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include <json-c/json.h>

#define MB (1024.0 * 1024.0)

int ipc_listen(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0)
        return -errno;
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", ZP_SOCK_PATH);
    unlink(ZP_SOCK_PATH);   /* önceki çalışmadan kalmış olabilir */
    /* İzin 0666: erişim denetimi dosya izniyle değil, her komutta yapılır (docs/05 §7). */
    if (bind(fd, (struct sockaddr *) &addr, sizeof addr) < 0 ||
        chmod(ZP_SOCK_PATH, 0666) < 0 || listen(fd, 8) < 0) {
        int e = errno;
        close(fd);
        return -e;
    }
    return fd;
}

void ipc_init_clients(struct zp_client *c, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        c[i].fd = -1;
        c[i].len = 0;
    }
}

int ipc_accept(struct zp_ctx *ctx)
{
    int cfd = accept4(ctx->lfd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (cfd < 0)
        return (errno == EAGAIN || errno == EINTR) ? 0 : -errno;

    struct ucred cr;
    socklen_t len = sizeof cr;
    if (getsockopt(cfd, SOL_SOCKET, SO_PEERCRED, &cr, &len) < 0) {
        int e = errno;
        close(cfd);
        return -e;
    }
    for (size_t i = 0; i < ZP_MAX_CLIENTS; i++) {
        struct zp_client *c = &ctx->clients[i];
        if (c->fd >= 0)
            continue;
        c->fd = cfd;
        c->pid = cr.pid;
        c->uid = cr.uid;
        c->role = ROLE_NONE;
        c->subscribed = false;
        c->len = 0;
        LOG_DBG("istemci bağlandı: pid=%d uid=%u", (int) cr.pid, (unsigned) cr.uid);
        return 0;
    }
    LOG_WARN("istemci sınırı (%d) doldu, bağlantı reddedildi", ZP_MAX_CLIENTS);
    close(cfd);
    return 0;
}

void ipc_close(struct zp_client *c)
{
    if (c->fd >= 0)
        close(c->fd);
    c->fd = -1;
    c->len = 0;
}

int ipc_send_line(struct zp_client *c, const char *json)
{
    struct iovec iov[2] = {
        { (void *) json, strlen(json) },
        { (void *) "\n", 1 },
    };
    struct msghdr mh = { .msg_iov = iov, .msg_iovlen = 2 };
    /* MSG_NOSIGNAL: istemci kapandıysa SIGPIPE ile servis ölmesin. */
    return sendmsg(c->fd, &mh, MSG_NOSIGNAL) < 0 ? -errno : 0;
}

void ipc_broadcast(struct zp_ctx *ctx, const char *json)
{
    for (size_t i = 0; i < ZP_MAX_CLIENTS; i++)
        if (ctx->clients[i].fd >= 0 && ctx->clients[i].subscribed)
            ipc_send_line(&ctx->clients[i], json);
}

/* ---- yanıt yardımcıları ---- */

static int reply(struct zp_client *c, json_object *o)
{
    int r = ipc_send_line(c, json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
    json_object_put(o);
    return r;
}

static json_object *ok_object(void)
{
    json_object *o = json_object_new_object();
    json_object_object_add(o, "ok", json_object_new_boolean(1));
    return o;
}

static int reply_ok(struct zp_client *c) { return reply(c, ok_object()); }

static int reply_error(struct zp_client *c, const char *err)
{
    json_object *o = json_object_new_object();
    json_object_object_add(o, "ok", json_object_new_boolean(0));
    json_object_object_add(o, "error", json_object_new_string(err));
    return reply(c, o);
}

static const char *get_str(json_object *msg, const char *key)
{
    json_object *v;
    if (!json_object_object_get_ex(msg, key, &v) || !json_object_is_type(v, json_type_string))
        return NULL;
    return json_object_get_string(v);
}

/* ---- komutlar ---- */

static int cmd_hello(struct zp_client *c, json_object *msg)
{
    const char *role = get_str(msg, "role");
    json_object *jp;
    int proto = json_object_object_get_ex(msg, "proto", &jp) ? json_object_get_int(jp) : 0;
    if (proto != ZP_PROTO)
        return reply_error(c, "unsupported protocol");
    if (role && strcmp(role, "agent") == 0)
        c->role = ROLE_AGENT;
    else if (role && strcmp(role, "gui") == 0)
        c->role = ROLE_GUI;
    json_object *o = ok_object();
    json_object_object_add(o, "proto", json_object_new_int(ZP_PROTO));
    return reply(c, o);
}

static int cmd_subscribe(struct zp_client *c)
{
    c->subscribed = true;
    return reply_ok(c);
}

static int cmd_status(struct zp_ctx *ctx, struct zp_client *c)
{
    json_object *o = ok_object();
    json_object_object_add(o, "enabled", json_object_new_boolean(ctx->cfg.enabled));
    json_object_object_add(o, "profile", json_object_new_string(ctx->prof->name));
    json_object_object_add(o, "level", json_object_new_int(ctx->pol.level));

    struct psi_avgs psi;
    if (psi_read_avgs(&psi) == 0) {
        json_object *p = json_object_new_object();
        json_object_object_add(p, "some10", json_object_new_double(psi.some10));
        json_object_object_add(p, "some60", json_object_new_double(psi.some60));
        json_object_object_add(p, "full10", json_object_new_double(psi.full10));
        json_object_object_add(o, "psi", p);
    }
    /* TODO(G-14): "mem" (MemTotal/MemAvailable, /proc/meminfo) ve zram_stats()
     * çalışınca "zram" alanları (docs/05 §6 status örneği). */

    json_object *apps = json_object_new_array();
    for (size_t i = 0; i < ctx->apps.n; i++) {
        const struct zp_app *a = &ctx->apps.a[i];
        json_object *j = json_object_new_object();
        json_object_object_add(j, "app", json_object_new_string(a->comm));
        json_object_object_add(j, "unit", json_object_new_string(a->unit));
        json_object_object_add(j, "mem_mb", json_object_new_int((int) (a->mem / MB)));
        json_object_object_add(j, "swap_mb", json_object_new_int((int) (a->swap / MB)));
        json_object_object_add(j, "frozen", json_object_new_boolean(a->frozen));
        json_object_object_add(j, "focused", json_object_new_boolean(a->pid == ctx->focused_pid));
        json_object_object_add(j, "never_freeze", json_object_new_boolean(a->never_freeze));
        json_object_object_add(j, "audio", json_object_new_boolean(a->audio));
        json_object_array_add(apps, j);
    }
    json_object_object_add(o, "apps", apps);
    return reply(c, o);
}

static int cmd_window(struct zp_ctx *ctx, struct zp_client *c, json_object *msg)
{
    (void) ctx; (void) msg;
    /* TODO(G-04, G-21): Sorumlu Furkan. docs/05 bölüm 3.2.
     *  - "pid" alanını al; auth_owns_pid(c->uid, pid) değilse yok say (G-52)
     *  - event "opened": excluded değilse apps_add, süreç ağacını topla (docs/06 §6.3),
     *    apps_unit_name + units_adopt, /proc/<pid>/cgroup değişene kadar ≤1 sn bekle,
     *    cg_path_of_pid ile app->cgroup'u doldur, last_focus = util_now()
     *    never_freeze bayrağını ctx->cfg.never_freeze listesinden ayarla
     *  - event "closed": tablodan kaldır (scope'u systemd kendisi siler) */
    return reply_error(c, "not implemented");
}

static int cmd_focus(struct zp_ctx *ctx, struct zp_client *c, json_object *msg)
{
    (void) ctx; (void) msg;
    /* TODO(G-07, G-22): auth_owns_pid kontrolü; ctx->focused_pid = pid;
     * uygulama donuksa HEMEN units_thaw, frozen=false, thaw süresini ölç,
     * LOG_EVT thawed ... thaw_ms=..., ipc_broadcast({"event":"thawed",...});
     * last_focus = util_now(). */
    return reply_error(c, "not implemented");
}

static int cmd_audio(struct zp_ctx *ctx, struct zp_client *c, json_object *msg)
{
    (void) ctx; (void) msg;
    /* TODO(G-23): Tüm uygulamalarda audio=false; "pids" dizisindeki, istemciye ait
     * her PID'in uygulamasında audio=true. */
    return reply_error(c, "not implemented");
}

static int cmd_admin(struct zp_ctx *ctx, struct zp_client *c, const char *cmd, json_object *msg)
{
    (void) msg;
    /* Ayar değiştiren komutlar polkit yetkisi ister (G-52). */
    int auth = auth_polkit_check(ctx->bus, c->pid);
    if (auth < 0)
        return reply_error(c, "authorization check failed");
    if (auth == 0)
        return reply_error(c, "not authorized");
    /* TODO(G-31, G-32, G-33): Sorumlu Furkan (servis tarafı).
     *  set_profile:  zp_apply_profile(ctx, name); cfg_save
     *  set_enabled:  false ise units_thaw_all + tüm frozen=false; cfg_save
     *  never_freeze: listeye ekle/çıkar, uygulama bayrağını güncelle; cfg_save */
    (void) cmd;
    return reply_error(c, "not implemented");
}

static int dispatch(struct zp_ctx *ctx, struct zp_client *c, const char *line)
{
    json_object *msg = json_tokener_parse(line);
    if (!msg || !json_object_is_type(msg, json_type_object)) {
        json_object_put(msg);
        LOG_WARN("geçersiz JSON, bağlantı kapatılıyor (pid %d)", (int) c->pid);
        return -EBADMSG;   /* T-33 */
    }
    const char *cmd = get_str(msg, "cmd");
    int r;
    if (!cmd)
        r = reply_error(c, "missing cmd");
    else if (strcmp(cmd, "hello") == 0)
        r = cmd_hello(c, msg);
    else if (strcmp(cmd, "status") == 0)
        r = cmd_status(ctx, c);
    else if (strcmp(cmd, "subscribe") == 0)
        r = cmd_subscribe(c);
    else if (strcmp(cmd, "window") == 0)
        r = cmd_window(ctx, c, msg);
    else if (strcmp(cmd, "focus") == 0)
        r = cmd_focus(ctx, c, msg);
    else if (strcmp(cmd, "audio") == 0)
        r = cmd_audio(ctx, c, msg);
    else if (strcmp(cmd, "set_profile") == 0 || strcmp(cmd, "set_enabled") == 0 ||
             strcmp(cmd, "never_freeze") == 0)
        r = cmd_admin(ctx, c, cmd, msg);
    else
        r = reply_error(c, "unknown command");
    json_object_put(msg);
    /* Yanıt gönderilemediyse (istemci kapanmış) bağlantıyı kapat. */
    return r < 0 ? r : 0;
}

int ipc_read(struct zp_ctx *ctx, struct zp_client *c)
{
    ssize_t r = recv(c->fd, c->buf + c->len, ZP_MAX_MSG - 1 - c->len, 0);
    if (r == 0)
        return -ECONNRESET;
    if (r < 0)
        return (errno == EAGAIN || errno == EINTR) ? 0 : -errno;
    c->len += (size_t) r;

    char *start = c->buf;
    char *nl;
    while ((nl = memchr(start, '\n', c->len - (size_t) (start - c->buf)))) {
        *nl = '\0';
        if (nl > start) {
            int d = dispatch(ctx, c, start);
            if (d < 0)
                return d;
        }
        start = nl + 1;
    }
    size_t rest = c->len - (size_t) (start - c->buf);
    memmove(c->buf, start, rest);
    c->len = rest;
    if (c->len >= ZP_MAX_MSG - 1) {
        LOG_WARN("mesaj %d bayt sınırını aştı, bağlantı kapatılıyor (pid %d)",
                 ZP_MAX_MSG, (int) c->pid);
        return -EMSGSIZE;   /* T-33 */
    }
    return 0;
}
