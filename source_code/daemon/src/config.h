/* Yapılandırma dosyası (/etc/zperf/zperf.conf) ve profil tablosu.
 * Profil parametreleri ve anlamları: docs/05 bölüm 5. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZP_CONF_PATH  "/etc/zperf/zperf.conf"
#define ZP_MAX_NAMES  16
#define ZP_NAME_LEN   32

struct zp_profile {
    const char *name;           /* "dengeli", "akilli-tahta", "ofis" */
    const char *algorithm;      /* zram sıkıştırma algoritması */
    int         swappiness;
    const char *k1_spec;        /* PSI tetikleyicisi, örn. "some 100000 1000000" */
    const char *k2_some_spec;   /* K2, some veya full'den biri ateşlenince */
    const char *k2_full_spec;
    double      exit_some10;    /* seviye düşürme eşiği (some avg10, %) */
    uint64_t    reclaim_step;   /* kademe 1'de tek seferde istenen bayt */
    int         idle_s;         /* dondurmak için en az boşta kalma süresi */
    bool        freeze;         /* false: yalnızca kademe 1 */
};

struct zp_config {
    char   profile[ZP_NAME_LEN];
    bool   enabled;
    char   never_freeze[ZP_MAX_NAMES][ZP_NAME_LEN];   /* comm adları */
    size_t n_never_freeze;
    char   exclude_extra[ZP_MAX_NAMES][ZP_NAME_LEN];  /* ek dışlama listesi */
    size_t n_exclude_extra;
};

/* Ada göre profil; bilinmeyen adda NULL. */
const struct zp_profile *cfg_profile(const char *name);
const struct zp_profile *cfg_default_profile(void);

void cfg_defaults(struct zp_config *c);
/* Dosya yoksa varsayılanlarla 0 döner. Bilinmeyen anahtar uyarıyla atlanır (U-08). */
int  cfg_load(struct zp_config *c, const char *path);
int  cfg_save(const struct zp_config *c, const char *path);

bool cfg_in_list(char list[][ZP_NAME_LEN], size_t n, const char *name);
