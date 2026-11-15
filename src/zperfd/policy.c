/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "policy.h"
#include "applist.h"
#include "cgroup.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

/* Reclaim adim boyutlari. Kucuk adimlar, senpai yaklasiminda oldugu gibi
 * baskiyi kademeli dusurur; EAGAIN (kismi) normaldir. */
#define RECLAIM_STEP_T1 (32ULL * 1024 * 1024)   /* kademe 1: 32 MB */
#define RECLAIM_STEP_T2 (128ULL * 1024 * 1024)  /* kademe 2: 128 MB */

static char g_focus[APP_NAME_MAX];

void policy_init(void)
{
    applist_init();
    g_focus[0] = '\0';
}

const char *policy_focused(void)
{
    return g_focus;
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
            LOGI("Odak: %s cozuldu", name);
        }
    }

    /* Onceki odaktaki uygulama artik odakta degil: odak birakma anini
     * isaretle ki LRU'da dogru yaslansin. */
    if (g_focus[0] && strcmp(g_focus, name) != 0)
        applist_mark_focus(g_focus);

    snprintf(g_focus, sizeof(g_focus), "%s", name);
}

int policy_on_tier(int tier)
{
    if (tier <= 0)
        return 0;

    int acted = 0;

    /* Kademe 1: odakta olmayan tum yonetilen uygulamalardan kucuk adim reclaim. */
    for (int i = 0; i < applist_count(); i++) {
        app_t *a = applist_at(i);
        if (!a->used || !a->managed)
            continue;
        if (g_focus[0] && strcmp(a->name, g_focus) == 0)
            continue;
        if (cgroup_app_reclaim(a->name, RECLAIM_STEP_T1) == 0)
            acted++;
    }

    /* Kademe 2: kurbani dondur + daha buyuk adim reclaim. */
    if (tier >= 2) {
        app_t *v = applist_pick_victim(g_focus);
        if (v && !v->frozen) {
            if (cgroup_app_freeze(v->name, 1) == 0) {
                v->frozen = 1;
                cgroup_app_reclaim(v->name, RECLAIM_STEP_T2);
                LOGI("Kademe 2: %s donduruldu ve sikistirildi", v->name);
                acted++;
            }
        }
    }

    return acted;
}
