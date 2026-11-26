/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "policy.h"
#include "applist.h"
#include "cgroup.h"
#include "profile.h"
#include "util.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

/* Bundan az bellegi olan uygulamaya kademe 1 reclaim uygulanmaz. */
#define RECLAIM_MIN_BYTES (4LL * 1024 * 1024)

static char g_focus[APP_NAME_MAX];
static int  g_enabled = 1;
static char g_protected[512];      /* ZPERF_PROTECT: asla dondurulmayacak adlar */

/* Enjekte edilen bagimliliklar (main.c baglar; birim testlerinde NULL). */
static void (*g_emit)(const char *ev, const char *name) = NULL;
static int  (*g_audio_playing)(pid_t pid) = NULL;

void policy_init(void)
{
    applist_init();
    g_focus[0] = '\0';
    g_enabled = 1;
}

void policy_set_event_cb(void (*fn)(const char *, const char *)) { g_emit = fn; }
void policy_set_audio_cb(int (*fn)(pid_t)) { g_audio_playing = fn; }

void policy_set_protected(const char *csv)
{
    if (csv)
        snprintf(g_protected, sizeof(g_protected), "%s", csv);
    else
        g_protected[0] = '\0';
}

/* name, ZPERF_PROTECT listesinde mi? Liste bosluk/virgulle ayrilir. */
static int is_protected(const char *name)
{
    if (!g_protected[0] || !name || !*name)
        return 0;
    size_t nl = strlen(name);
    for (const char *p = g_protected; *p; ) {
        while (*p == ' ' || *p == ',' || *p == '\t')
            p++;
        const char *start = p;
        while (*p && *p != ' ' && *p != ',' && *p != '\t')
            p++;
        if ((size_t)(p - start) == nl && strncmp(start, name, nl) == 0)
            return 1;
    }
    return 0;
}

const char *policy_focused(void) { return g_focus; }
int policy_enabled(void) { return g_enabled; }

static void emit(const char *ev, const char *name)
{
    if (g_emit)
        g_emit(ev, name);
}

void policy_set_enabled(int on)
{
    on = on ? 1 : 0;
    if (on == g_enabled)
        return;
    g_enabled = on;
    LOGI("Yonetim %s", on ? "acildi" : "kapatildi");
    if (!on) {
        /* Kapatilinca geriye donmus uygulama birakma. */
        for (int i = 0; i < applist_count(); i++) {
            app_t *a = applist_at(i);
            if (a->used && a->frozen && cgroup_app_freeze(a->name, 0) == 0) {
                a->frozen = 0;
                emit("thawed", a->name);
            }
        }
    }
}

void policy_on_focus(const char *name, pid_t pid)
{
    if (!name || !*name)
        return;

    /* Kendi denetim arayuzumuzu (zperf-gui) ne yonetime al ne de odak say:
     * panele tiklamak, kullanicinin o an korunan gercek uygulamasinin
     * odagini calmasin. Boylece panel acikken son gercek odak korunur. */
    if (strncmp(name, "zperf", 5) == 0)
        return;

    app_t *a = applist_get(name);
    if (!a) {
        /* Yeni uygulama: yonetime al ve cgroup'a tasi.
         * NOT (test bulgusu): cgroup v2 var olan sayfalari tasimada yuklemez;
         * bu yuzden reclaim en iyi, uygulama erken (ilk odakta) alindiginda
         * calisir. Freeze ise her kosulda calisir. */
        a = applist_add(name, pid);
        if (a) {
            cgroup_app_create(name);
            int moved = cgroup_app_move_tree(name, pid);
            a->managed = (moved > 0);
            LOGI("Yonetime alindi: %s (pid %d, %d surec tasindi)",
                 name, (int)pid, moved);
        }
    } else {
        a->pid = pid;
    }

    /* Korunan uygulamalari (ZPERF_PROTECT; or. surucunun VSCode'u/terminali)
     * "asla dondurma" olarak isaretle; panelde de isaretli gorunur. */
    if (a && is_protected(name))
        a->never_freeze = 1;

    /* Odak alan uygulama donmussa hemen coz (oneri: kademe 0 davranisi). */
    if (a && a->frozen) {
        if (cgroup_app_freeze(name, 0) == 0) {
            a->frozen = 0;
            emit("thawed", name);
            LOGI("Odak: %s cozuldu", name);
        }
    }

    /* Onceki odaktaki uygulama artik odakta degil: odak birakma anini
     * isaretle ki LRU'da dogru yaslansin. */
    if (g_focus[0] && strcmp(g_focus, name) != 0)
        applist_mark_focus(g_focus);

    snprintf(g_focus, sizeof(g_focus), "%s", name);
}

/* Kurban secimi: odakta olmayan, "asla dondurma" olmayan, donmamis, korunmayan
 * ve ses calmayan adaylar arasinda SU AN EN COK BELLEK TUKETENI secer. Bellek
 * her cagrida guncel okunur; "asla dondurma" bayragi da canli kontrol edildigi
 * icin koruma kaldirilinca uygulama aninda yeniden aday olur. */
static app_t *pick_victim(const char *focus)
{
    app_t *best = NULL;
    long long best_mem = -1;

    for (int i = 0; i < applist_count(); i++) {
        app_t *a = applist_at(i);
        if (!a->used || a->frozen || a->never_freeze)
            continue;
        if (is_protected(a->name))          /* savunma amacli ikinci kontrol */
            continue;
        if (focus && focus[0] && strcmp(a->name, focus) == 0)
            continue;
        if (g_audio_playing && a->pid > 0 && g_audio_playing(a->pid)) {
            LOGD("Kurban atlandi (ses caliyor): %s", a->name);
            continue;
        }

        long long mem = cgroup_app_mem_current(a->name);
        if (mem < 0)
            mem = 0;
        if (mem > best_mem) {
            best = a;
            best_mem = mem;
        }
    }
    return best;
}

int policy_on_tier(int tier)
{
    if (!g_enabled || tier <= 0)
        return 0;

    const profile_t *pf = profile_current();
    int acted = 0;

    /* Kademe 1: odakta olmayan tum yonetilen uygulamalardan kucuk adim reclaim. */
    for (int i = 0; i < applist_count(); i++) {
        app_t *a = applist_at(i);
        if (!a->used || !a->managed)
            continue;
        if (a->frozen)          /* donmus cgroup'a reclaim yazmak servisi
                                 * kilitleyebilir; dokunma (zaten sikistirildi). */
            continue;
        if (g_focus[0] && strcmp(a->name, g_focus) == 0)
            continue;
        /* Bosaltilacak bellegi kalmamis (or. zaten swap'e itilmis donmus)
         * uygulamaya reclaim yazmak bosuna cekirdek isidir. */
        if (cgroup_app_mem_current(a->name) < RECLAIM_MIN_BYTES)
            continue;
        if (cgroup_app_reclaim(a->name, pf->reclaim_step_t1) == 0)
            acted++;
    }

    /* Kademe 2: kurbani dondur + daha buyuk adim reclaim. */
    if (tier >= 2) {
        app_t *v = pick_victim(g_focus);
        if (v && !v->frozen) {
            /* ONEMLI (test bulgusu): once CANLI cgroup'u sikistir, SONRA dondur.
             * Donmus bir cgroup'a memory.reclaim yazmak cekirdekte suresiz
             * bloke olup tek thread'li olay dongusunu (IPC + bakim + odak)
             * tamamen kilitleyebiliyor. Sira tersine cevrildi. */
            cgroup_app_reclaim(v->name, pf->reclaim_step_t2);
            if (cgroup_app_freeze(v->name, 1) == 0) {
                v->frozen = 1;
                v->frozen_at_ms = util_now_ms();
                emit("frozen", v->name);
                LOGI("Kademe 2: %s sikistirildi ve donduruldu", v->name);
                acted++;
            }
        }
    }

    return acted;
}

void policy_thaw(const char *name)
{
    if (!name || !*name)
        return;
    app_t *a = applist_get(name);
    if (!a || !a->frozen)
        return;
    if (cgroup_app_freeze(name, 0) == 0) {
        a->frozen = 0;
        emit("thawed", name);
        LOGI("Elle cozuldu: %s", name);
    }
}

void policy_set_never_freeze(const char *name, int on)
{
    applist_set_never_freeze(name, on ? 1 : 0);
    /* "Asla dondurma" isaretlenince, uygulama su an donmussa hemen coz:
     * kullanici "bunu asla dondurma" diyorsa donuk kalmasi tutarsiz olur. */
    if (on)
        policy_thaw(name);
}

void policy_maintenance(void)
{
    if (!g_enabled)
        return;

    const profile_t *pf = profile_current();
    long now = util_now_ms();
    long max_ms = (long)pf->thaw_max_frozen_sec * 1000;

    for (int i = 0; i < applist_count(); i++) {
        app_t *a = applist_at(i);
        if (!a->used || !a->frozen)
            continue;
        if (now - a->frozen_at_ms >= max_ms) {
            if (cgroup_app_freeze(a->name, 0) == 0) {
                a->frozen = 0;
                emit("thawed", a->name);
                LOGI("Periyodik coz: %s (%d sn'den uzun donmustu)",
                     a->name, pf->thaw_max_frozen_sec);
            }
        }
    }
}
