/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AUDIO_MAX   128
#define CACHE_SEC   2          /* onbellek tazeleme araligi */

static pid_t  g_pids[AUDIO_MAX];
static int    g_n = 0;
static time_t g_ts = 0;
static int    g_unavailable = 0;   /* pactl calismadi: bir daha ugrasma */

/* `pactl list sink-inputs` ciktisindan calan (corked olmayan) akislarin
 * application.process.id degerlerini toplar. */
static void refresh(void)
{
    time_t now = time(NULL);
    if (g_unavailable)
        return;
    if (now - g_ts < CACHE_SEC)
        return;
    g_ts = now;
    g_n = 0;

    FILE *f = popen("pactl list sink-inputs 2>/dev/null", "r");
    if (!f) {
        g_unavailable = 1;
        return;
    }

    char line[512];
    long cur_pid = -1;
    int  corked = 0;
    int  saw_any = 0;
    while (fgets(line, sizeof(line), f)) {
        saw_any = 1;
        if (strstr(line, "Sink Input #")) {  /* yeni akis blogu basladi */
            if (cur_pid > 0 && !corked && g_n < AUDIO_MAX)
                g_pids[g_n++] = (pid_t)cur_pid;
            cur_pid = -1;
            corked = 0;
            continue;
        }
        if (strstr(line, "Corked: yes"))
            corked = 1;
        char *p = strstr(line, "application.process.id");
        if (p) {
            p = strchr(p, '"');
            if (p)
                cur_pid = strtol(p + 1, NULL, 10);
        }
    }
    /* son blok */
    if (cur_pid > 0 && !corked && g_n < AUDIO_MAX)
        g_pids[g_n++] = (pid_t)cur_pid;

    int rc = pclose(f);
    if (rc != 0 && !saw_any)
        g_unavailable = 1;   /* pactl yok / baglanamadi */
}

int audio_is_playing(pid_t pid)
{
    if (pid <= 0)
        return 0;
    refresh();
    for (int i = 0; i < g_n; i++)
        if (g_pids[i] == pid)
            return 1;
    return 0;
}
