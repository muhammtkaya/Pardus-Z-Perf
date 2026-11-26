/* systemd ile uygulama grupları: geçici scope oluşturma, dondurma, çözme (TK-02).
 * TK-02 deneyi (docs/06 bölüm 7.1) başarısız olursa bu dosyanın içi cgroupfs ile
 * yeniden yazılır; arayüz (fonksiyon imzaları) değişmez. */
#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <systemd/sd-bus.h>

#define ZP_UNIT_PREFIX "zperf-"

/* pids dizisindeki süreçleri "unit" adlı yeni bir scope'a alır (Slice=user-<uid>.slice,
 * MemoryAccounting=yes). Taşıma eşzamansızdır; çağıran taraf /proc/<pid>/cgroup
 * değişene kadar bekler. */
int units_adopt(sd_bus *bus, const char *unit, const pid_t *pids, size_t n, uid_t uid);

int units_freeze(sd_bus *bus, const char *unit);
int units_thaw(sd_bus *bus, const char *unit);

/* Adı ZP_UNIT_PREFIX ile başlayan tüm scope'ları çözer (G-08). */
int units_thaw_all(sd_bus *bus);
