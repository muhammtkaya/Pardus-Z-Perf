/* zram takas aygıtının kurulumu ve istatistikleri (G-02, TK-07).
 * Kaynak: Linux çekirdek belgeleri, admin-guide/blockdev/zram. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "config.h"

struct zram_stats {
    char     device[32];      /* "/dev/zram0" */
    char     algorithm[16];
    uint64_t disksize;        /* bayt */
    uint64_t orig_data;       /* mm_stat alan 1: sıkıştırılmamış boyut */
    uint64_t compr_data;      /* mm_stat alan 2: sıkıştırılmış boyut */
    uint64_t mem_used;        /* mm_stat alan 3: toplam kullanılan bellek */
};

/* Takas olarak kullanılan bir zram aygıtı varsa 1, yoksa 0 (/proc/swaps). */
int zram_active(char *device, size_t n);

/* zram yoksa profile göre kurar; varsa dokunmaz. vm.swappiness ve page-cluster'ı
 * ayarlar; ilk değiştirmeden önce eski swappiness'i ZP_STATE_DIR'e kaydeder. */
int zram_setup(const struct zp_profile *p);

int zram_stats(struct zram_stats *out);

#define ZP_STATE_DIR "/var/lib/zperf"
