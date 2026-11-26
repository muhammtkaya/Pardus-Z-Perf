/* zperfd: Z-Performans servisi. Tek iş parçacıklı poll() döngüsü (docs/05 bölüm 1):
 * signalfd, timerfd (1 sn), PSI tetikleyicileri, dinleme soketi, istemciler. */
#include "zperfd.h"
#include "cgroupfs.h"
#include "log.h"
#include "psi.h"
#include "units.h"
#include "util.h"
#include "zram.h"

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>

/* Statik: istemci tamponları toplamda 1 MB'ı aşıyor, yığına konmaz. */
static struct zp_ctx ctx;

static void close_psi_triggers(struct zp_ctx *c)
{
    for (int k = 0; k < PSI_NFDS; k++) {
        if (c->psi_fd[k] >= 0)
            close(c->psi_fd[k]);
        c->psi_fd[k] = -1;
    }
}

static int open_psi_triggers(struct zp_ctx *c)
{
    const char *spec[PSI_NFDS] = {
        [PSI_K1]      = c->prof->k1_spec,
        [PSI_K2_SOME] = c->prof->k2_some_spec,
        [PSI_K2_FULL] = c->prof->k2_full_spec,
    };
    close_psi_triggers(c);
    for (int k = 0; k < PSI_NFDS; k++) {
        int fd = psi_trigger_open(spec[k]);
        if (fd < 0) {
            LOG_ERR("PSI tetikleyicisi '%s' kurulamadı: %s", spec[k], strerror(-fd));
            close_psi_triggers(c);
            return fd;
        }
        c->psi_fd[k] = fd;
    }
    return 0;
}

int zp_apply_profile(struct zp_ctx *c, const char *name)
{
    const struct zp_profile *p = cfg_profile(name);
    if (!p)
        return -EINVAL;
    c->prof = p;
    snprintf(c->cfg.profile, sizeof c->cfg.profile, "%s", p->name);
    int r = open_psi_triggers(c);
    /* TODO(G-12): vm.swappiness'i yeni profile göre ayarla. Algoritma değişikliği
     * bir sonraki açılışta geçerli olur (TK-09). */
    LOG_EVT("profile name=%s t=%.3f", p->name, util_wallclock());
    return r;
}

static int setup_signals(void)
{
    sigset_t m;
    sigemptyset(&m);
    sigaddset(&m, SIGTERM);
    sigaddset(&m, SIGINT);
    if (sigprocmask(SIG_BLOCK, &m, NULL) < 0)
        return -errno;
    int fd = signalfd(-1, &m, SFD_CLOEXEC);
    return fd < 0 ? -errno : fd;
}

static int setup_timer(void)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (fd < 0)
        return -errno;
    struct itimerspec its = { .it_interval = { 1, 0 }, .it_value = { 1, 0 } };
    if (timerfd_settime(fd, 0, &its, NULL) < 0) {
        int e = errno;
        close(fd);
        return -e;
    }
    return fd;
}

static void apply_action(const struct policy_action *a)
{
    struct zp_app *app = &ctx.apps.a[a->app_idx];
    switch (a->kind) {
    case ACT_RECLAIM:
        /* TODO(G-05, G-15): öncesi memory.current; cg_reclaim(app->cgroup, a->bytes);
         * sonrası memory.current; farkı
         * LOG_EVT("reclaim app=%s unit=%s mb=%.1f t=%.3f", ...) ile yaz. */
        (void) app;
        break;
    case ACT_FREEZE:
        /* TODO(G-06, G-15, G-24): units_freeze; cg_wait_frozen(app->cgroup, 500);
         * app->mem'in %80'ini 64 MB'lık parçalarla cg_reclaim; app->frozen = true;
         * LOG_EVT("frozen app=%s unit=%s reclaimed_mb=%.0f t=%.3f", ...);
         * ipc_broadcast(&ctx, "{\"event\":\"frozen\",...}"). */
        break;
    }
}

static void on_tick(int k1, int k2)
{
    struct policy_input in = { 0 };
    in.k1_fired = k1;
    in.k2_fired = k2;
    in.now = util_now();
    in.enabled = ctx.cfg.enabled;
    if (psi_read_avgs(&in.psi) < 0)
        LOG_WARN("PSI ortalamaları okunamadı");

    /* TODO(G-05): Her uygulama için cg_read_u64(app->cgroup, "memory.current" ve
     * "memory.swap.current") ile mem/swap'ı güncelle; scope'u kalmamış (okuma
     * -ENOENT) uygulamaları apps_remove ile tablodan çıkar. */

    in.apps = ctx.apps.a;
    in.napps = ctx.apps.n;
    in.focused_idx = -1;
    for (size_t i = 0; i < ctx.apps.n; i++)
        if (ctx.apps.a[i].pid == ctx.focused_pid)
            in.focused_idx = (int) i;

    int old_level = ctx.pol.level;
    struct policy_action act[ZP_MAX_APPS + 1];
    int n = policy_step(&ctx.pol, ctx.prof, &in, act, sizeof act / sizeof act[0]);

    if (ctx.pol.level != old_level) {
        char ev[160];
        LOG_EVT("level from=%d to=%d some10=%.2f full10=%.2f t=%.3f", old_level,
                ctx.pol.level, in.psi.some10, in.psi.full10, util_wallclock());
        snprintf(ev, sizeof ev,
                 "{\"event\":\"level\",\"from\":%d,\"to\":%d,\"some10\":%.2f,\"full10\":%.2f}",
                 old_level, ctx.pol.level, in.psi.some10, in.psi.full10);
        ipc_broadcast(&ctx, ev);
    }
    for (int i = 0; i < n; i++)
        apply_action(&act[i]);
}

static void usage(const char *prog)
{
    fprintf(stderr, "Kullanım: %s [-d] [-c yapılandırma]\n"
                    "  -d  hata ayıklama günlüğü\n"
                    "  -c  yapılandırma dosyası (varsayılan " ZP_CONF_PATH ")\n", prog);
}

int main(int argc, char **argv)
{
    int opt;
    ctx.conf_path = ZP_CONF_PATH;
    while ((opt = getopt(argc, argv, "dc:h")) != -1) {
        switch (opt) {
        case 'd': g_debug = 1; break;
        case 'c': ctx.conf_path = optarg; break;
        default:  usage(argv[0]); return opt == 'h' ? 0 : 2;
        }
    }

    for (int k = 0; k < PSI_NFDS; k++)
        ctx.psi_fd[k] = -1;
    ctx.sfd = ctx.tfd = ctx.lfd = -1;
    ipc_init_clients(ctx.clients, ZP_MAX_CLIENTS);
    policy_init(&ctx.pol);

    int r = cfg_load(&ctx.cfg, ctx.conf_path);
    if (r < 0)
        LOG_WARN("%s okunamadı (%s), varsayılanlar kullanılıyor", ctx.conf_path, strerror(-r));

    r = sd_bus_open_system(&ctx.bus);
    if (r < 0) {
        LOG_ERR("system bus'a bağlanılamadı: %s", strerror(-r));
        return 1;
    }

    if (zram_setup(cfg_profile(ctx.cfg.profile)) < 0)
        LOG_WARN("zram kurulamadı; disk takasıyla devam ediliyor (T-31)");

    /* Önceki bir çalışmadan donuk kalmış uygulama olmasın (G-08). */
    units_thaw_all(ctx.bus);

    if (zp_apply_profile(&ctx, ctx.cfg.profile) < 0) {
        LOG_ERR("PSI kullanılamıyor; çekirdekte PSI açık mı? (T-30)");
        return 1;
    }

    ctx.sfd = setup_signals();
    ctx.tfd = setup_timer();
    ctx.lfd = ipc_listen();
    if (ctx.sfd < 0 || ctx.tfd < 0 || ctx.lfd < 0) {
        LOG_ERR("başlatma başarısız: signalfd=%d timerfd=%d soket=%d", ctx.sfd, ctx.tfd, ctx.lfd);
        return 1;
    }

    sd_notify(0, "READY=1");
    LOG_INFO("zperfd başladı: profil=%s politika=%s", ctx.prof->name,
             ctx.cfg.enabled ? "açık" : "kapalı");

    for (;;) {
        struct pollfd p[3 + PSI_NFDS + ZP_MAX_CLIENTS];
        int client_of[ZP_MAX_CLIENTS];
        size_t n = 0;

        p[n++] = (struct pollfd) { ctx.sfd, POLLIN, 0 };
        p[n++] = (struct pollfd) { ctx.tfd, POLLIN, 0 };
        for (int k = 0; k < PSI_NFDS; k++)
            p[n++] = (struct pollfd) { ctx.psi_fd[k], POLLPRI, 0 };   /* -1 ise poll yok sayar */
        p[n++] = (struct pollfd) { ctx.lfd, POLLIN, 0 };
        size_t first_client = n;
        for (int i = 0; i < ZP_MAX_CLIENTS; i++) {
            if (ctx.clients[i].fd >= 0) {
                client_of[n - first_client] = i;
                p[n++] = (struct pollfd) { ctx.clients[i].fd, POLLIN, 0 };
            }
        }

        if (poll(p, n, -1) < 0) {
            if (errno == EINTR)
                continue;
            LOG_ERR("poll: %s", strerror(errno));
            break;
        }

        if (p[0].revents & POLLIN) {
            LOG_INFO("kapanış sinyali alındı");
            break;
        }

        int tick = 0, k1 = 0, k2 = 0;
        if (p[1].revents & POLLIN) {
            uint64_t expirations;
            ssize_t rr = read(ctx.tfd, &expirations, sizeof expirations);
            (void) rr;
            tick = 1;
        }
        for (int k = 0; k < PSI_NFDS; k++) {
            short ev = p[2 + k].revents;
            if (ev & POLLERR) {
                /* Tetikleyici geçersiz oldu; sürekli POLLERR ile döngü dönmesin. */
                LOG_ERR("PSI tetikleyicisi %d geçersiz, kapatıldı", k);
                close(ctx.psi_fd[k]);
                ctx.psi_fd[k] = -1;
            } else if (ev & POLLPRI) {
                if (k == PSI_K1)
                    k1 = 1;
                else
                    k2 = 1;
            }
        }
        if (p[2 + PSI_NFDS].revents & POLLIN)
            ipc_accept(&ctx);
        for (size_t j = first_client; j < n; j++) {
            if (p[j].revents & (POLLIN | POLLHUP | POLLERR)) {
                struct zp_client *c = &ctx.clients[client_of[j - first_client]];
                if (ipc_read(&ctx, c) < 0)
                    ipc_close(c);
            }
        }

        if (tick || k1 || k2)
            on_tick(k1, k2);
    }

    sd_notify(0, "STOPPING=1");
    units_thaw_all(ctx.bus);   /* G-08: kapanırken kimse donuk kalmasın */
    for (int i = 0; i < ZP_MAX_CLIENTS; i++)
        ipc_close(&ctx.clients[i]);
    close_psi_triggers(&ctx);
    close(ctx.lfd);
    unlink(ZP_SOCK_PATH);
    close(ctx.tfd);
    close(ctx.sfd);
    sd_bus_flush_close_unref(ctx.bus);
    LOG_INFO("zperfd durdu");
    return 0;
}
