/* SPDX-License-Identifier: GPL-3.0-or-later
 * cgroup.c mekanizmasinin uctan uca testi (root + cgroup v2 gerektirir).
 * Bellek tutan bir cocuk surec yaratir, onu bir uygulama cgroup'una tasir,
 * dondurur, geri kazandirir ve temizler; her adimi dogrular.
 *
 * Derleme:
 *   gcc -I src/zperfd tests/unit/test_cgroup.c \
 *       src/zperfd/cgroup.c src/zperfd/util.c src/zperfd/log.c -o /tmp/test_cgroup
 */
#include "cgroup.h"
#include "log.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define APP "test"
#define HOG_MB 256

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  [GECTI] %s\n", msg); \
    else { printf("  [KALDI] %s\n", msg); fails++; } \
} while (0)

/* Cocuk surec: ONCE SIGUSR1 bekler (bu arada ebeveyn onu cgroup'a tasir),
 * SONRA ~HOG_MB MB ayirip dokunur. Boylece sayfalar dogrudan uygulama
 * cgroup'una yuklenir (cgroup v2 var olan sayfalari tasimada yuklemez). */
static void child_hog(void)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    /* SIGUSR1 ebeveynde zaten blokludur (fork'tan once); burada bekle. */
    int sig;
    sigwait(&set, &sig);

    size_t n = (size_t)HOG_MB * 1024 * 1024;
    char *p = malloc(n);
    if (!p)
        _exit(1);
    memset(p, 1, n);          /* sayfalari gercekten yerlesik yap */
    for (;;) pause();         /* oldurulene kadar bekle */
    _exit(0);
}

int main(void)
{
    if (geteuid() != 0) { printf("Bu test root gerektirir.\n"); return 77; }
    log_set_level(LOG_INFO_);

    printf("== cgroup_init ==\n");
    CHECK(cgroup_init() == 0, "cgroup tabani olusturuldu");

    printf("== uygulama cgroup'u ==\n");
    CHECK(cgroup_app_create(APP) == 0, "app-test olusturuldu");

    /* SIGUSR1'i fork'tan ONCE blokla ki cocuk kaybetmesin (yaris onleme). */
    sigset_t uset;
    sigemptyset(&uset);
    sigaddset(&uset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &uset, NULL);

    pid_t child = fork();
    if (child == 0) { child_hog(); _exit(0); }
    CHECK(child > 0, "cocuk surec yaratildi");

    printf("== tasima (bellek AYRILMADAN once) ==\n");
    CHECK(cgroup_app_move_pid(APP, child) == 0, "PID cgroup'a tasindi");

    /* Simdi cocuk bellegi ayirsin; sayfalar app-test'e yuklenecek. */
    kill(child, SIGUSR1);
    sleep(2);

    long long before = cgroup_app_mem_current(APP);
    printf("  memory.current (tasima sonrasi) = %lld bayt (~%lld MB)\n",
           before, before / (1024 * 1024));
    CHECK(before > 100LL * 1024 * 1024, "cgroup bellegi > 100 MB gorunuyor");

    printf("== dondurma ==\n");
    CHECK(cgroup_app_freeze(APP, 1) == 0, "dondurma komutu verildi");
    usleep(300000);
    CHECK(cgroup_is_frozen(APP) == 1, "cgroup.events frozen=1");

    printf("== geri kazanim (memory.reclaim) ==\n");
    int rc = cgroup_app_reclaim(APP, (unsigned long long)128 * 1024 * 1024);
    long long after = cgroup_app_mem_current(APP);
    printf("  memory.current (reclaim sonrasi) = %lld bayt (~%lld MB)\n",
           after, after / (1024 * 1024));
    if (rc == 0)
        CHECK(after < before, "reclaim sonrasi bellek azaldi (sayfalar swap'e itildi)");
    else
        printf("  [NOT] memory.reclaim yok/basarisiz (memory denetleyicisi kapali olabilir)\n");

    printf("== cozme ==\n");
    CHECK(cgroup_app_freeze(APP, 0) == 0, "cozme komutu verildi");
    usleep(300000);
    CHECK(cgroup_is_frozen(APP) == 0, "cgroup.events frozen=0");

    printf("== temizlik ==\n");
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    CHECK(cgroup_app_destroy(APP) == 0, "app-test silindi");
    cgroup_cleanup_all();

    printf("\n%s\n", fails == 0 ? "TUM TESTLER GECTI" : "BAZI TESTLER KALDI");
    return fails == 0 ? 0 : 1;
}
