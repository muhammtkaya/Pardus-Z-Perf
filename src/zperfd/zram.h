/* SPDX-License-Identifier: GPL-3.0-or-later
 * zram.h: zram sikistirilmis takas aygitinin kurulumu ve sokulmesi.
 *
 * Pardus 25.0'de zram varsayilan olarak kapalidir; bu modul servis
 * acilisinda yeni bir aygit olusturur, takas olarak etkinlestirir ve
 * servis cikarken geri alir. Sistemde zaten etkin bir zram takasi varsa
 * ona dokunmaz (onerideki "kuruluysa mevcut aygiti kullan" kurali).
 */
#ifndef ZPERF_ZRAM_H
#define ZPERF_ZRAM_H

#include <stdint.h>

typedef struct {
    int      device_id;      /* zram aygit numarasi; -1 = aygit yok */
    int      owned;          /* 1 = bu servis olusturdu, cikista sokulecek */
    char     dev_path[64];   /* ornek: /dev/zram0 */
    uint64_t disksize;       /* ayarlanan mantiksal boyut (bayt) */
    char     algorithm[32];  /* ornek: lz4 */
} zram_dev_t;

/* zram takasini hazirlar.
 *   algorithm   : "lz4", "zstd" vb. (NULL -> "lz4")
 *   ram_percent : disksize = MemTotal * percent / 100
 *   priority    : takas onceligi (zram'in disk takasindan onde olmasi icin yuksek)
 * Basarili olursa 0 doner ve *out doldurulur. */
int zram_setup(zram_dev_t *out, const char *algorithm,
               unsigned ram_percent, int priority);

/* Servisin olusturdugu zram aygitini takas disi birakip kaldirir.
 * Baskasinin kurdugu aygita (owned == 0) dokunmaz. */
void zram_teardown(zram_dev_t *dev);

#endif /* ZPERF_ZRAM_H */
