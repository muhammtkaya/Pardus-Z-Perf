/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "policy.h"
#include "applist.h"
#include "cgroup.h"
#include "profile.h"
#include "util.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

static char g_focus[APP_NAME_MAX];
static int  g_enabled = 1;

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

/* Kurban secimi: odakta olmayan, "asla dondurma" olmayan, donmamis ve ses
 * calmayan adaylar arasinda en yuksek skorlu. Skor = yas * (bellek + taban);
 * taban sayesinde bellek esit/sifirsa secim saf LRU'ya (en eski) duser. */
static app_t *pick_victim(const char *focus)
{
    app_t *best = NULL;
    double best_score = -1.0;
    long now = util_now_ms();

    for (int i = 0; i < applist_count(); i++) {
        app_t *a = applist_at(i);
        if (!a->used || a->frozen || a->never_freeze)
            continue;
        if (focus && focus[0] && strcmp(a->name, focus) == 0)
            continue;
        if (g_audio_playing && a->pid > 0 && g_audio_playing(a->pid)) {
            LOGD("Kurban atlandi (ses caliyor): %s", a->name);
            continue;
        }

        long age = now - a->last_focus_ms;
        if (age < 0)
            age = 0;
        long long mem = cgroup_app_mem_current(a->name);
        if (mem < 0)
            mem = 0;
        /* 16 MB taban: bellek bilinmezse/sifirsa yas belirleyici olur. */
        double score = (double)age * ((double)mem + 16.0 * 1024 * 1024);
        if (!best || score > best_score) {
            best = a;
            best_score = score;
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
        if (g_focus[0] && strcmp(a->name, g_focus) == 0)
            continue;
        if (cgroup_app_reclaim(a->name, pf->reclaim_step_t1) == 0)
            acted++;
    }

    /* Kademe 2: kurbani dondur + daha buyuk adim reclaim. */
    if (tier >= 2) {
        app_t *v = pick_victim(g_focus);
        if (v && !v->frozen) {
            if (cgroup_app_freeze(v->name, 1) == 0) {
                v->frozen = 1;
                v->frozen_at_ms = util_now_ms();
                cgroup_app_reclaim(v->name, pf->reclaim_step_t2);
                emit("frozen", v->name);
                LOGI("Kademe 2: %s donduruldu ve sikistirildi", v->name);
                acted++;
            }
        }
    }

    return acted;
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
