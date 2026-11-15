/* SPDX-License-Identifier: GPL-3.0-or-later
 * Politika motoru uctan uca testi (root + cgroup v2 gerektirir).
 * Iki "uygulama" (cocuk surec) yaratir, odagi A'dan B'ye kaydirir, kademe 2
 * uygular ve en uzun suredir odaksiz olan A'nin donduruldugunu; A'ya geri
 * odaklaninca cozuldugunu dogrular.
 *
 * Derleme:
 *   gcc -I src/zperfd tests/unit/test_policy.c src/zperfd/policy.c \
 *       src/zperfd/applist.c src/zperfd/cgroup.c src/zperfd/util.c \
 *       src/zperfd/log.c -o /tmp/test_policy
 */
#include "policy.h"
#include "applist.h"
#include "cgroup.h"
#include "log.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, m) do { if (c) printf("  [GECTI] %s\n", m); \
    else { printf("  [KALDI] %s\n", m); fails++; } } while (0)

static pid_t spawn_sleeper(void)
{
    pid_t p = fork();
    if (p == 0) { for (;;) pause(); _exit(0); }
    return p;
}

int main(void)
{
    if (geteuid() != 0) { printf("Bu test root gerektirir.\n"); return 77; }
    log_set_level(LOG_INFO_);

    cgroup_init();
    policy_init();

    pid_t A = spawn_sleeper();
    pid_t B = spawn_sleeper();
    CHECK(A > 0 && B > 0, "iki uygulama sureci yaratildi");

    printf("== odak A, sonra B ==\n");
    policy_on_focus("appA", A);
    usleep(20000);
    policy_on_focus("appB", B);
    CHECK(strcmp(policy_focused(), "appB") == 0, "odak appB'de");

    printf("== kademe 2 uygulanir ==\n");
    int acted = policy_on_tier(2);
    usleep(300000);
    CHECK(acted >= 1, "politika en az bir eylem yapti");
    CHECK(cgroup_is_frozen("appA") == 1, "en eski (appA) donduruldu");
    CHECK(cgroup_is_frozen("appB") == 0, "odaktaki (appB) donmadi");

    printf("== appA'ya geri odaklan ==\n");
    policy_on_focus("appA", A);
    usleep(300000);
    CHECK(cgroup_is_frozen("appA") == 0, "appA odak alinca cozuldu");

    printf("== temizlik ==\n");
    kill(A, SIGKILL); kill(B, SIGKILL);
    waitpid(A, NULL, 0); waitpid(B, NULL, 0);
    cgroup_cleanup_all();

    printf("\n%s\n", fails == 0 ? "TUM TESTLER GECTI" : "BAZI TESTLER KALDI");
    return fails ? 1 : 0;
}
