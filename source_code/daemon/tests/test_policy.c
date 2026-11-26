/* policy_step birim testleri (docs/07 bölüm 2, U-01 – U-07).
 * Testler, politika yazılmadan ÖNCE davranışı tanımlamak için yazıldı; policy.c
 * tamamlanana kadar KALDI görmeniz beklenir. */
#include "../src/policy.h"
#include "test.h"

#include <string.h>

enum { FOX, PAD, OFFICE, PLAYER, KEEP, NAPPS };
#define MB (1024ULL * 1024ULL)
#define NOW 1000.0

static struct zp_app apps[NAPPS];
static struct policy_action act[16];

/* FOX odakta; PAD ve OFFICE 100 sn'dir boşta (OFFICE en büyük);
 * PLAYER ses çalıyor; KEEP "asla dondurma" listesinde. */
static void setup(void)
{
    memset(apps, 0, sizeof apps);
    const char *names[NAPPS] = { "firefox-esr", "mousepad", "soffice.bin", "vlc", "obs" };
    const uint64_t mem[NAPPS] = { 900 * MB, 150 * MB, 400 * MB, 600 * MB, 700 * MB };
    for (int i = 0; i < NAPPS; i++) {
        apps[i].pid = 100 + i;
        snprintf(apps[i].comm, sizeof apps[i].comm, "%s", names[i]);
        snprintf(apps[i].unit, sizeof apps[i].unit, "zperf-x-%d.scope", 100 + i);
        snprintf(apps[i].cgroup, sizeof apps[i].cgroup, "/user.slice/zperf-x-%d.scope", 100 + i);
        apps[i].mem = mem[i];
        apps[i].last_focus = NOW - 100;
    }
    apps[FOX].last_focus = NOW;
    apps[PLAYER].audio = true;
    apps[KEEP].never_freeze = true;
}

static struct policy_input input(int k1, int k2, double now, double some10, double full10)
{
    struct policy_input in = { 0 };
    in.k1_fired = k1;
    in.k2_fired = k2;
    in.now = now;
    in.psi.some10 = some10;
    in.psi.full10 = full10;
    in.apps = apps;
    in.napps = NAPPS;
    in.focused_idx = FOX;
    in.enabled = 1;
    return in;
}

static int count(int n, enum policy_act_kind kind)
{
    int c = 0;
    for (int i = 0; i < n; i++)
        c += act[i].kind == kind;
    return c;
}

static int touches(int n, int idx)
{
    for (int i = 0; i < n; i++)
        if (act[i].app_idx == idx)
            return 1;
    return 0;
}

static int frozen_idx(int n)
{
    for (int i = 0; i < n; i++)
        if (act[i].kind == ACT_FREEZE)
            return act[i].app_idx;
    return -1;
}

static void u01_k1_starts_reclaim(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    struct policy_input in = input(1, 0, NOW, 12, 0);
    int n = policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(s.level == 1);
    CHECK(count(n, ACT_RECLAIM) >= 1);
    CHECK(count(n, ACT_FREEZE) == 0);
    for (int i = 0; i < n; i++)
        CHECK(act[i].bytes > 0 && act[i].bytes <= cfg_profile("dengeli")->reclaim_step);
}

static void u02_k2_freezes_largest_idle(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    struct policy_input in = input(1, 1, NOW, 30, 6);
    int n = policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(s.level == 2);
    CHECK(count(n, ACT_FREEZE) == 1);
    /* PLAYER ses çalıyor, KEEP korunuyor, FOX odakta → en büyük aday OFFICE */
    CHECK(frozen_idx(n) == OFFICE);
}

static void u03_focused_untouched(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    for (int step = 0; step < 5; step++) {
        struct policy_input in = input(1, step > 1, NOW + 10.0 * step, 30, 6);
        int n = policy_step(&s, cfg_profile("akilli-tahta"), &in, act, 16);
        CHECK(!touches(n, FOX));
    }
}

static void u04_protected_never_frozen(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    apps[OFFICE].excluded = true;   /* artık en büyük uygun aday PAD */
    struct policy_input in = input(1, 1, NOW, 30, 6);
    int n = policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(frozen_idx(n) == PAD);
    for (int i = 0; i < n; i++)
        if (act[i].kind == ACT_FREEZE)
            CHECK(act[i].app_idx != PLAYER && act[i].app_idx != KEEP && act[i].app_idx != OFFICE);
}

static void u05_freeze_interval(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    struct policy_input in = input(1, 1, NOW, 30, 6);
    int n = policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(count(n, ACT_FREEZE) == 1);
    apps[frozen_idx(n) < 0 ? OFFICE : frozen_idx(n)].frozen = true;

    in = input(0, 1, NOW + 2, 30, 6);              /* 2 sn sonra: dondurma yok */
    n = policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(count(n, ACT_FREEZE) == 0);

    in = input(0, 1, NOW + ZP_FREEZE_INTERVAL_S + 1, 30, 6);   /* aralık doldu */
    n = policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(count(n, ACT_FREEZE) == 1);
    CHECK(frozen_idx(n) == PAD);                   /* OFFICE zaten donuk */
}

static void u06_hysteresis(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    struct policy_input in = input(1, 0, NOW, 12, 0);
    policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(s.level == 1);

    in = input(0, 0, NOW + 1, 1, 0);               /* baskı düştü */
    policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    in = input(0, 0, NOW + 5, 1, 0);               /* 4 sn: henüz inme */
    policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(s.level == 1);
    in = input(0, 0, NOW + 1 + ZP_HYSTERESIS_S + 1, 1, 0);
    policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(s.level == 0);
}

static void u07_office_never_freezes(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    struct policy_input in = input(1, 1, NOW, 40, 10);
    int n = policy_step(&s, cfg_profile("ofis"), &in, act, 16);
    CHECK(s.level == 2);
    CHECK(count(n, ACT_FREEZE) == 0);
    CHECK(count(n, ACT_RECLAIM) >= 1);
}

static void disabled_does_nothing(void)
{
    struct policy_state s;
    policy_init(&s);
    setup();
    struct policy_input in = input(1, 1, NOW, 40, 10);
    in.enabled = 0;
    int n = policy_step(&s, cfg_profile("dengeli"), &in, act, 16);
    CHECK(n == 0);
}

int main(void)
{
    RUN(u01_k1_starts_reclaim);
    RUN(u02_k2_freezes_largest_idle);
    RUN(u03_focused_untouched);
    RUN(u04_protected_never_frozen);
    RUN(u05_freeze_interval);
    RUN(u06_hysteresis);
    RUN(u07_office_never_freezes);
    RUN(disabled_does_nothing);
    return SUMMARY();
}
