/* SPDX-License-Identifier: GPL-3.0-or-later
 * util.h: sysfs/procfs yazma-okuma ve alt surec calistirma yardimcilari.
 */
#ifndef ZPERF_UTIL_H
#define ZPERF_UTIL_H

#include <stddef.h>
#include <stdint.h>

/* Bir dosyaya tek bir dize yazar (sysfs icin). 0 basarili, -1 hata (errno). */
int util_write_str(const char *path, const char *value);

/* Bir dosyadan en fazla (len-1) bayt okur, sonuna '\0' koyar.
 * Okunan bayt sayisini dondurur, hata halinde -1. */
long util_read_str(const char *path, char *buf, size_t len);

/* Bir yolun var olup olmadigini soyler (access F_OK). */
int util_exists(const char *path);

/* argv[0]'i PATH uzerinden bulup calistirir (kabuk yok), bitmesini bekler.
 * Cikis kodunu dondurur; baslatilamazsa -1. */
int util_run(char *const argv[]);

/* /proc/meminfo icindeki MemTotal degerini bayt cinsinden dondurur (0 = hata). */
uint64_t util_total_ram_bytes(void);

/* /proc/swaps icinde adinda "zram" gecen etkin bir takas var mi? */
int util_zram_swap_active(void);

/* Monotonik saat, milisaniye (odak yasi / LRU hesaplari icin). */
long util_now_ms(void);

#endif /* ZPERF_UTIL_H */
