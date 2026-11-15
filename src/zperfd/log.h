/* SPDX-License-Identifier: GPL-3.0-or-later
 * zperfd - Pardus Z-Performans bellek yoneticisi servisi
 * log.h: Basit, seviyeli gunluk kaydi (systemd journal'a stderr uzerinden).
 */
#ifndef ZPERF_LOG_H
#define ZPERF_LOG_H

/* Seviyeler syslog onceligine karsilik gelir; journald "<N>" onekini okur. */
typedef enum {
    LOG_ERR_   = 3,
    LOG_WARN_  = 4,
    LOG_INFO_  = 6,
    LOG_DEBUG_ = 7,
} log_level_t;

/* Calisma aninda gosterilecek en dusuk seviye (DEBUG dahil etmek icin ayarlanir). */
void log_set_level(log_level_t max_level);

void log_msg(log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define LOGE(...) log_msg(LOG_ERR_,   __VA_ARGS__)
#define LOGW(...) log_msg(LOG_WARN_,  __VA_ARGS__)
#define LOGI(...) log_msg(LOG_INFO_,  __VA_ARGS__)
#define LOGD(...) log_msg(LOG_DEBUG_, __VA_ARGS__)

#endif /* ZPERF_LOG_H */
