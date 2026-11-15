/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "applist.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static app_t g_apps[APP_MAX];

void applist_init(void)
{
    memset(g_apps, 0, sizeof(g_apps));
}

app_t *applist_get(const char *name)
{
    for (int i = 0; i < APP_MAX; i++)
        if (g_apps[i].used && strcmp(g_apps[i].name, name) == 0)
            return &g_apps[i];
    return NULL;
}

app_t *applist_add(const char *name, pid_t pid)
{
    app_t *a = applist_get(name);
    if (a) {              /* zaten var: PID'i tazele */
        a->pid = pid;
        return a;
    }
    for (int i = 0; i < APP_MAX; i++) {
        if (!g_apps[i].used) {
            memset(&g_apps[i], 0, sizeof(app_t));
            g_apps[i].used = 1;
            g_apps[i].pid = pid;
            g_apps[i].last_focus_ms = util_now_ms();
            snprintf(g_apps[i].name, APP_NAME_MAX, "%s", name);
            return &g_apps[i];
        }
    }
    return NULL;          /* yer yok */
}

void applist_mark_focus(const char *name)
{
    app_t *a = applist_get(name);
    if (a)
        a->last_focus_ms = util_now_ms();
}

void applist_set_never_freeze(const char *name, int on)
{
    app_t *a = applist_get(name);
    if (a)
        a->never_freeze = on;
}

app_t *applist_pick_victim(const char *focused_name)
{
    app_t *best = NULL;
    for (int i = 0; i < APP_MAX; i++) {
        app_t *a = &g_apps[i];
        if (!a->used || a->frozen || a->never_freeze)
            continue;
        if (focused_name && strcmp(a->name, focused_name) == 0)
            continue;
        if (!best || a->last_focus_ms < best->last_focus_ms)
            best = a;      /* en eski odak = en uzun suredir kullanilmiyor */
    }
    return best;
}

int applist_count(void)
{
    return APP_MAX;
}

app_t *applist_at(int i)
{
    if (i < 0 || i >= APP_MAX)
        return NULL;
    return &g_apps[i];
}
