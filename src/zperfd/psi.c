/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "psi.h"
#include "util.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PSI_MEMORY_PATH "/proc/pressure/memory"

psi_trigger_t psi_register(psi_kind_t kind, int tier,
                           uint64_t threshold_us, uint64_t window_us)
{
    psi_trigger_t t = {
        .fd = -1, .kind = kind, .tier = tier,
        .threshold_us = threshold_us, .window_us = window_us,
    };

    int fd = open(PSI_MEMORY_PATH, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LOGE("PSI acilamadi (%s): %s", PSI_MEMORY_PATH, strerror(errno));
        return t;
    }

    const char *name = (kind == PSI_FULL) ? "full" : "some";
    char line[64];
    int n = snprintf(line, sizeof(line), "%s %llu %llu", name,
                     (unsigned long long)threshold_us,
                     (unsigned long long)window_us);
    if (n < 0 || (size_t)n >= sizeof(line)) {
        close(fd);
        return t;
    }

    if (write(fd, line, (size_t)n) < 0) {
        LOGE("PSI tetikleyici yazilamadi ('%s'): %s", line, strerror(errno));
        close(fd);
        return t;
    }

    t.fd = fd;
    LOGI("PSI tetikleyici kaydedildi (kademe %d): %s", tier, line);
    return t;
}

void psi_close(psi_trigger_t *t)
{
    if (t->fd >= 0) {
        close(t->fd);
        t->fd = -1;
    }
}

long psi_read_current(char *buf, size_t len)
{
    return util_read_str(PSI_MEMORY_PATH, buf, len);
}

double psi_avg10(const char *content, psi_kind_t kind)
{
    const char *name = (kind == PSI_FULL) ? "full" : "some";
    const char *line = strstr(content, name);
    if (!line)
        return -1.0;

    const char *p = strstr(line, "avg10=");
    if (!p)
        return -1.0;

    double v = -1.0;
    if (sscanf(p + strlen("avg10="), "%lf", &v) != 1)
        return -1.0;
    return v;
}
