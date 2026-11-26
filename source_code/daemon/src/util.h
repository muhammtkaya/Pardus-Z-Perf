/* /proc ve /sys dosyaları için okuma/yazma yardımcıları, alt süreç çalıştırma, saat.
 * Tüm fonksiyonlar başarıda 0 (veya pozitif değer), hatada -errno döndürür. */
#pragma once

#include <stddef.h>
#include <sys/types.h>

/* Dosyanın içeriğini buf'a okur ve NUL ile bitirir. Okunan bayt sayısını döndürür. */
int util_read_file(const char *path, char *buf, size_t n);

/* s'yi tek bir write() ile yazar. Kısmi yazım hata sayılır (-EIO). */
int util_write_file(const char *path, const char *s);

/* argv[0] tam yol olmalı. Çıkış kodu 0 ise 0, değilse -EIO. */
int util_run(char *const argv[]);

/* /proc/<pid>/comm ve /proc/<pid>/status içindeki gerçek UID. */
int util_pid_comm(pid_t pid, char *buf, size_t n);
int util_pid_uid(pid_t pid, uid_t *uid);

/* CLOCK_MONOTONIC (karar zamanlaması) ve CLOCK_REALTIME (günlük damgası), saniye. */
double util_now(void);
double util_wallclock(void);
