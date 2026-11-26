/* Günlük makroları. Servis stderr'e yazar; systemd bunu journald'a iletir ve
 * satır başındaki <n> önekini günlük düzeyi olarak tanır (sd-daemon(3)). */
#pragma once

#include <stdio.h>

extern int g_debug;

#define LOG_ERR(fmt, ...)  fprintf(stderr, "<3>" fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) fprintf(stderr, "<4>" fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) fprintf(stderr, "<6>" fmt "\n", ##__VA_ARGS__)
#define LOG_DBG(fmt, ...) \
    do { if (g_debug) fprintf(stderr, "<7>" fmt "\n", ##__VA_ARGS__); } while (0)

/* Ölçüm betiklerinin ayrıştırdığı olay satırları; biçim docs/06 bölüm 4'te.
 * Örnek: EVT frozen app=mousepad unit=zperf-mousepad-2401.scope reclaimed_mb=40 t=... */
#define LOG_EVT(fmt, ...)  fprintf(stderr, "<6>EVT " fmt "\n", ##__VA_ARGS__)
