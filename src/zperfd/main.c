/* SPDX-License-Identifier: GPL-3.0-or-later
 * zperfd - Pardus Z-Performans servisi
 *
 *   1. zram sikistirilmis takas aygitini kurar (yoksa olusturur).
 *   2. PSI tetikleyicileriyle bellek baskisini olay-guduml olarak izler.
 *   3. Baski kademesine gore politika uygular: kademe 1 odakta olmayan
 *      uygulamalardan kucuk reclaim, kademe 2 en eski uygulamayi dondur +
 *      sikistir (policy.c); odak gelince cozer.
 *   4. /run/zperf.sock uzerinden ajan/arayuzle konusur (ipc.c).
 *   5. SIGTERM/SIGINT alinca her seyi temiz birakir (cozer, zram'i soker).
 */
#include "applist.h"
#include "audio.h"
#include "cgroup.h"
#include "ipc.h"
#include "log.h"
#include "policy.h"
#include "profile.h"
#include "psi.h"
#include "util.h"
#include "zram.h"

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#define MAX_TRIGGERS 4
#define STATUS_PERIOD_SEC 5
#define EV_MAX 16
#define ZPERF_SOCK "/run/zperf.sock"
/* PSI tetikleyicileri baski altinda saniyede onlarca kez uyandirabilir
 * (Pardus 6.12'de olculdu: ~18/sn). Politikayi her uyanista calistirmak
 * gereksiz reclaim cagrisi ve CPU harcar; en fazla bu aralikta bir calistir. */
#define POLICY_MIN_INTERVAL_MS 1000

/* Kademe siniflandirma esikleri artik gecerli PROFILDEN okunur (profile.h):
 * uyandiktan sonra okunan avg10, profilin tier1/tier2 esikleriyle kiyaslanir.
 * Esikler PSI tetikleyici esikleri DEGILdir; profille calisma zamani degisir. */

/* --- Varsayilan ayarlar (onerideki kademelere yakin baslangic degerleri).
 * Esikler 6. haftadaki olcumlerle ayarlanacak. --- */
static const char   *g_algorithm   = "lz4";
static unsigned      g_ram_percent = 100;
static int           g_swap_prio   = 100;
static int           g_setup_zram  = 1;

static void usage(const char *prog)
{
    fprintf(stderr,
        "Kullanim: %s [secenekler]\n"
        "  --algorithm <ad>    zram sikistirma algoritmasi (varsayilan: lz4)\n"
        "  --ram-percent <n>   zram boyutu = RAM'in %%n'i (varsayilan: 100)\n"
        "  --swap-prio <n>     zram takas onceligi (varsayilan: 100)\n"
        "  --no-zram           zram kurmadan yalnizca PSI izle\n"
        "  --debug             ayrintili gunluk\n"
        "  --help              bu yardim\n",
        prog);
}

static int parse_args(int argc, char **argv)
{
    static const struct option opts[] = {
        { "algorithm",   required_argument, 0, 'a' },
        { "ram-percent", required_argument, 0, 'r' },
        { "swap-prio",   required_argument, 0, 'p' },
        { "no-zram",     no_argument,       0, 'n' },
        { "debug",       no_argument,       0, 'd' },
        { "help",        no_argument,       0, 'h' },
        { 0, 0, 0, 0 },
    };
    int c;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (c) {
        case 'a': g_algorithm = optarg; break;
        case 'r': g_ram_percent = (unsigned)atoi(optarg); break;
        case 'p': g_swap_prio = atoi(optarg); break;
        case 'n': g_setup_zram = 0; break;
        case 'd': log_set_level(LOG_DEBUG_); break;
        case 'h': usage(argv[0]); exit(0);
        default:  usage(argv[0]); return -1;
        }
    }
    return 0;
}

/* Kapanis sinyalleri icin bir signalfd olusturur. */
static int make_signal_fd(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        LOGE("sigprocmask: %s", strerror(errno));
        return -1;
    }
    int fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (fd < 0)
        LOGE("signalfd: %s", strerror(errno));
    return fd;
}

/* Periyodik durum logu icin bir timerfd olusturur. */
static int make_timer_fd(int period_sec)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (fd < 0) {
        LOGE("timerfd_create: %s", strerror(errno));
        return -1;
    }
    struct itimerspec its = {
        .it_interval = { .tv_sec = period_sec, .tv_nsec = 0 },
        .it_value    = { .tv_sec = period_sec, .tv_nsec = 0 },
    };
    if (timerfd_settime(fd, 0, &its, NULL) < 0) {
        LOGE("timerfd_settime: %s", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

/* avg10 degerlerine gore politika kademesini belirler (gecerli profilden). */
static int classify_tier(double some, double full)
{
    const profile_t *pf = profile_current();
    if (some >= pf->tier2_some_pct || full >= pf->tier2_full_pct)
        return 2;
    if (some >= pf->tier1_some_pct)
        return 1;
    return 0;
}

/* Anlik baskiyi okur, kademeyi hesaplar, loglar ve kademeyi dondurur. */
static int report_pressure(const char *reason)
{
    char buf[512];
    if (psi_read_current(buf, sizeof(buf)) < 0)
        return -1;
    double some = psi_avg10(buf, PSI_SOME);
    double full = psi_avg10(buf, PSI_FULL);
    int tier = classify_tier(some, full);
    LOGI("%s | some=%.2f%% full=%.2f%% -> KADEME %d", reason, some, full, tier);
    return tier;
}

/* --- IPC geri cagirmalari --- */
static void focus_cb(const char *name, pid_t pid)
{
    LOGI("IPC odak bildirimi: %s (pid %d)", name, (int)pid);
    policy_on_focus(name, pid);
}

static void status_cb(char *buf, size_t len)
{
    char p[512];
    double some = -1, full = -1;
    if (psi_read_current(p, sizeof(p)) >= 0) {
        some = psi_avg10(p, PSI_SOME);
        full = psi_avg10(p, PSI_FULL);
    }
    int off = snprintf(buf, len,
             "{\"some\":%.2f,\"full\":%.2f,\"focus\":\"%s\","
             "\"profile\":\"%s\",\"enabled\":%d,\"apps\":[",
             some, full, policy_focused(), profile_name(), policy_enabled());
    if (off < 0 || (size_t)off >= len)
        return;

    /* Yonetilen uygulamalar: panel bunlari listeler (donuk mu, sabit mi).
     * Kendi arayuzumuz (zperf-*) listeye alinmaz. Tampon dolarsa kalan
     * uygulamalar atlanir ama JSON yine de gecerli kapanir. */
    int first = 1;
    for (int i = 0; i < applist_count(); i++) {
        app_t *a = applist_at(i);
        if (!a || !a->used)
            continue;
        if (strncmp(a->name, "zperf", 5) == 0)
            continue;
        long long mem = cgroup_app_mem_current(a->name);
        if (mem < 0)
            mem = 0;
        int n = snprintf(buf + off, len - (size_t)off,
                 "%s{\"name\":\"%s\",\"frozen\":%d,\"never_freeze\":%d,\"mem\":%lld}",
                 first ? "" : ",", a->name, a->frozen ? 1 : 0,
                 a->never_freeze ? 1 : 0, mem);
        if (n < 0 || (size_t)n >= len - (size_t)off)
            break;                 /* tampon doldu: kalanlari atla */
        off += n;
        first = 0;
    }
    if ((size_t)off + 2 < len) {
        buf[off++] = ']';
        buf[off++] = '}';
        buf[off] = '\0';
    }
}

static void cmd_cb(const char *cmd, const char *name, int value)
{
    if (strcmp(cmd, "never_freeze") == 0) {
        policy_set_never_freeze(name, value);   /* on iken donuksa ayrica cozer */
        LOGI("Ayar: %s 'asla dondurma' = %d", name, value);
    } else if (strcmp(cmd, "profile") == 0) {
        if (profile_apply_named(name) == 0)
            LOGI("Profil degisti: %s", profile_name());
        else
            LOGW("Bilinmeyen profil: %s", name);
    } else if (strcmp(cmd, "enabled") == 0) {
        policy_set_enabled(value);
    } else if (strcmp(cmd, "thaw") == 0) {
        policy_thaw(name);
    }
}

/* Politika olaylarini (dondu/cozuldu) bagli ajanlara yayinlar. */
static void emit_event(const char *ev, const char *name)
{
    char b[160];
    snprintf(b, sizeof(b), "{\"type\":\"event\",\"event\":\"%s\",\"name\":\"%s\"}",
             ev, name);
    ipc_broadcast(b);
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) != 0)
        return 1;

    if (geteuid() != 0) {
        LOGE("zperfd root yetkisiyle calismalidir.");
        return 1;
    }

    LOGI("zperfd baslatiliyor.");

    /* --- cgroup tabani + politika --- */
    if (cgroup_init() != 0)
        LOGW("cgroup tabani hazirlanamadi; dondurma/geri kazanim calismayabilir.");
    policy_init();
    profile_apply_named("Dengeli");       /* baslangic profili */
    policy_set_event_cb(emit_event);      /* dondu/cozuldu -> ajanlara yayinla */
    policy_set_audio_cb(audio_is_playing); /* ses calan uygulamayi kurban secme */
    policy_set_protected(getenv("ZPERF_PROTECT")); /* asla dondurulmayacak adlar */
    LOGI("Aktif profil: %s", profile_name());

    /* --- zram kurulumu --- */
    zram_dev_t zram = { .device_id = -1 };
    if (g_setup_zram) {
        if (zram_setup(&zram, g_algorithm, g_ram_percent, g_swap_prio) != 0)
            LOGW("zram kurulamadi; PSI izlemeye yine de devam edilecek.");
    } else {
        LOGI("--no-zram: zram kurulumu atlandi.");
    }

    /* --- PSI uyandirma tetikleyicileri ---
     * CEKIRDEK KISITI (testle dogrulandi): tetikleyici esigi, pencerenin
     * en fazla %10'u olabilir; daha yuksek yuzde EINVAL verir. Bu yuzden
     * tetikleyici yuksek kademeyi ifade edemez. Cozum: dusuk esikli bir
     * "uyandirma" sinyali kur (pencere 1 sn, esik %5 = 50000 us); baski
     * olunca uyan, sonra avg10 degerlerini OKUYUP kademeyi belirle. */
    psi_trigger_t triggers[MAX_TRIGGERS];
    int ntrig = 0;
    triggers[ntrig++] = psi_register(PSI_SOME, 1, 50000, 1000000);
    triggers[ntrig++] = psi_register(PSI_FULL, 2, 50000, 1000000);

    /* --- Olay dongusu altyapisi --- */
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    int sigfd = make_signal_fd();
    int timfd = make_timer_fd(STATUS_PERIOD_SEC);

    int rc = 1;
    if (epfd < 0 || sigfd < 0 || timfd < 0)
        goto cleanup;

    for (int i = 0; i < ntrig; i++) {
        if (triggers[i].fd < 0)
            continue;
        struct epoll_event ev = { .events = EPOLLPRI,
                                  .data = { .ptr = &triggers[i] } };
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, triggers[i].fd, &ev) < 0)
            LOGW("PSI fd epoll'a eklenemedi: %s", strerror(errno));
    }
    {
        struct epoll_event ev = { .events = EPOLLIN, .data = { .fd = sigfd } };
        epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &ev);
        ev.data.fd = timfd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, timfd, &ev);
    }

    /* --- IPC soketi (ajan/arayuz baglanir) --- */
    if (ipc_init(epfd, ZPERF_SOCK, focus_cb, status_cb) != 0)
        LOGW("IPC baslatilamadi; odak bildirimleri alinamayacak.");
    ipc_set_cmd_handler(cmd_cb);
    {   /* Yalnizca test: kök-baslatilan GUI polkit "active" oturumda gorunmez;
         * ZPERF_TRUST_UID verildiyse o uid polkit'siz ayar yapabilir. */
        const char *tu = getenv("ZPERF_TRUST_UID");
        if (tu && *tu)
            ipc_set_trust_uid(atol(tu));
    }

    LOGI("Baski izleme ve politika motoru aktif.");
    report_pressure("Baslangic durumu");

    int running = 1;
    long last_policy_ms = -POLICY_MIN_INTERVAL_MS;
    long suppressed = 0;
    while (running) {
        struct epoll_event events[EV_MAX];
        int n = epoll_wait(epfd, events, EV_MAX, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            LOGE("epoll_wait: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < n && running; i++) {
            uint32_t ev = events[i].events;

            /* PSI tetikleyici (EPOLLPRI, data.ptr) */
            if (ev & EPOLLPRI) {
                psi_trigger_t *t = events[i].data.ptr;
                const char *kind = (t->kind == PSI_FULL) ? "full" : "some";
                long now = util_now_ms();
                if (now - last_policy_ms < POLICY_MIN_INTERVAL_MS) {
                    suppressed++;       /* yakin zamanda zaten islendi */
                    continue;
                }
                last_policy_ms = now;
                if (suppressed)
                    LOGI(">> BASKI OLAYI (%s tetikleyici; son %ld uyanis birlestirildi)",
                         kind, suppressed);
                else
                    LOGI(">> BASKI OLAYI (%s tetikleyici uyandirdi)", kind);
                suppressed = 0;
                int tier = report_pressure("   deger");
                policy_on_tier(tier);   /* kademe 1: reclaim, 2: dondur+sikistir */
                continue;
            }

            int fd = events[i].data.fd;
            if (fd == sigfd) {
                struct signalfd_siginfo si;
                if (read(sigfd, &si, sizeof(si)) == (ssize_t)sizeof(si))
                    LOGI("Sinyal alindi (%u); kapaniyor.", si.ssi_signo);
                running = 0;
            } else if (fd == timfd) {
                uint64_t exp;
                if (read(timfd, &exp, sizeof(exp)) != sizeof(exp)) { /* yok */ }
                policy_maintenance();   /* uzun donmus uygulamalari kisa sure coz */
                int tier = report_pressure("Periyodik durum");
                policy_on_tier(tier);
            } else if (ipc_owns_fd(fd)) {
                ipc_handle_fd(fd);      /* baglanti/odak/durum mesajlari */
            }
        }
    }
    rc = 0;

cleanup:
    LOGI("Temizlik yapiliyor.");
    ipc_shutdown();
    /* GUVENLIK: once tum donmus uygulamalari coz (geriye donuk kalmasin). */
    cgroup_cleanup_all();
    for (int i = 0; i < ntrig; i++)
        psi_close(&triggers[i]);
    if (timfd >= 0) close(timfd);
    if (sigfd >= 0) close(sigfd);
    if (epfd  >= 0) close(epfd);
    zram_teardown(&zram);
    LOGI("zperfd durdu.");
    return rc;
}
