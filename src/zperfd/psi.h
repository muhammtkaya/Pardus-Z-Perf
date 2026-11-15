/* SPDX-License-Identifier: GPL-3.0-or-later
 * psi.h: Linux Baski Durma Bilgisi (PSI) tetikleyicileri.
 *
 * /proc/pressure/memory dosyasina bir esik yazilir; cekirdek, verilen
 * pencere icinde o kadar durma birikince dosyayi "okunabilir" (POLLPRI)
 * yapar. Boylece surekli yoklama yapmadan, baski basladigi an uyaniriz.
 *
 * Not: Tetikleyiciler mutlak durma suresi (mikrosaniye) ve pencere ister;
 * dosyadaki avg10 yuzdesini dogrudan almaz. "some avg10 >= %10" yaklasik
 * olarak 1 sn penceresinde 100000 us durma esigiyle ifade edilir.
 */
#ifndef ZPERF_PSI_H
#define ZPERF_PSI_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PSI_SOME = 0,  /* en az bir gorev bellek yuzunden bekledi */
    PSI_FULL = 1,  /* tum gorevler bellek yuzunden bekledi */
} psi_kind_t;

typedef struct {
    int        fd;        /* poll/epoll edilecek dosya tanimlayicisi */
    psi_kind_t kind;
    int        tier;      /* bu tetikleyicinin ait oldugu politika kademesi */
    uint64_t   threshold_us;
    uint64_t   window_us;
} psi_trigger_t;

/* Bir PSI tetikleyicisi kaydeder. Basarili olursa fd >= 0 olan bir yapi
 * doner; kernel/izin hatasinda fd = -1. */
psi_trigger_t psi_register(psi_kind_t kind, int tier,
                           uint64_t threshold_us, uint64_t window_us);

void psi_close(psi_trigger_t *t);

/* /proc/pressure/memory anlik degerlerini buf'a okur (loglama icin). */
long psi_read_current(char *buf, size_t len);

/* avg10 yuzdesini ("some" veya "full" satirindan) ayiklar; -1 hata. */
double psi_avg10(const char *content, psi_kind_t kind);

#endif /* ZPERF_PSI_H */
