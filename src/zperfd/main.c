/* SPDX-License-Identifier: GPL-3.0-or-later
 * zperfd - Pardus Z-Performans servisi (ILK ASAMA: iskelet)
 *
 * Bu asamada servis sunlari yapar:
 *   1. zram sikistirilmis takas aygitini kurar (yoksa olusturur).
 *   2. PSI tetikleyicileriyle bellek baskisini olay-guduml olarak izler.
 *   3. Baski olaylarini ve periyodik durumu gunluge yazar.
 *   4. SIGTERM/SIGINT alinca her seyi temiz birakir (zram'i soker).
 *
 * cgroup tasima/dondurma, memory.reclaim, politika motoru ve IPC
 * sonraki asamalarda eklenecektir (bkz. docs/architecture.md).
 */
#include "applist.h"
#include "cgroup.h"
#include "ipc.h"
#include "log.h"
#include "policy.h"
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

/* Kademe siniflandirma esikleri (avg10 yuzdesi), oneriye gore.
 * Bunlar PSI tetikleyici esikleri DEGIL; uyandiktan sonra okunan avg10'a
 * uygulanan karar esikleridir. 6. haftadaki olcumlerle ayarlanacak. */
#define TIER1_SOME_PCT 10.0   /* kademe 1: proaktif sikistirma */
#define TIER2_SOME_PCT 25.0   /* kademe 2: dondur + sikistir     */
#define TIER2_FULL_PCT  5.0

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

/* avg10 degerlerine gore politika kademesini belirler (oneri Tablo). */
static int classify_tier(double some, double full)
{
    if (some >= TIER2_SOME_PCT || full >= TIER2_FULL_PCT)
        return 2;
    if (some >= TIER1_SOME_PCT)
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
    snprintf(buf, len, "{\"some\":%.2f,\"full\":%.2f,\"focus\":\"%s\"}",
             some, full, policy_focused());
}

static void cmd_cb(const char *cmd, const char *name, int value)
{
    if (strcmp(cmd, "never_freeze") == 0) {
        applist_set_never_freeze(name, value);
        LOGI("Ayar: %s 'asla dondurma' = %d", name, value);
    }
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) != 0)
        return 1;

    if (geteuid() != 0) {
        LOGE("zperfd root yetkisiyle calismalidir.");
        return 1;
    }

    LOGI("zperfd baslatiliyor (iskelet asamasi).");

    /* --- cgroup tabani + politika --- */
    if (cgroup_init() != 0)
        LOGW("cgroup tabani hazirlanamadi; dondurma/geri kazanim calismayabilir.");
    policy_init();

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

    LOGI("Baski izleme ve politika motoru aktif.");
    report_pressure("Baslangic durumu");

    int running = 1;
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
                LOGI(">> BASKI OLAYI (%s tetikleyici uyandirdi)", kind);
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
