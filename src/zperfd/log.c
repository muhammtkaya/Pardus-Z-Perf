/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static log_level_t g_max_level = LOG_INFO_;

void log_set_level(log_level_t max_level)
{
    g_max_level = max_level;
}

void log_msg(log_level_t level, const char *fmt, ...)
{
    if (level > g_max_level)
        return;

    /* systemd journald, stderr satirinin basindaki "<N>" onekini oncelik
     * olarak yorumlar. Terminalden calistirildiginda zararsizdir. */
    fprintf(stderr, "<%d>", (int)level);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
    fflush(stderr);
}
