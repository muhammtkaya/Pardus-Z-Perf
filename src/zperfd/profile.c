/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "profile.h"
#include "log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZPERF_PROFILE_DIR
#define ZPERF_PROFILE_DIR "/etc/zperf/profiles"
#endif

#define MB (1024ULL * 1024ULL)

/* --- Gomulu varsayilan profiller ---
 * Esikler ve adim boyutlari oneriye yakin baslangic degerleridir; 6. haftadaki
 * A/B olcumleriyle ayarlanacaktir (bkz. tests/measure). */
static const profile_t k_builtin[] = {
    /* Dengeli: genel amacli varsayilan. */
    { "Dengeli",      10.0, 25.0, 5.0, 32 * MB, 128 * MB, 45 },
    /* Ofis: daha az mudahaleci (yazi/tablo akiciligi onemli, ani reclaim az). */
    { "Ofis",         15.0, 35.0, 8.0, 16 * MB,  96 * MB, 90 },
    /* Akilli Tahta: etkilesimli pano; RAM'i hizla bosalt, donma suresi kisa. */
    { "Akilli Tahta",  7.0, 18.0, 4.0, 64 * MB, 192 * MB, 30 },
};
#define N_BUILTIN ((int)(sizeof(k_builtin) / sizeof(k_builtin[0])))

static profile_t g_current = { "Dengeli", 10.0, 25.0, 5.0, 32 * MB, 128 * MB, 45 };
static char g_dir[512] = ZPERF_PROFILE_DIR;

/* Ad -> slug: kucuk harf, alfasayisal disi -> '-', bastaki/sondaki '-' atilir. */
static void slugify(const char *in, char *out, size_t len)
{
    size_t j = 0;
    int prev_dash = 1;  /* bastaki '-'leri yut */
    for (const char *p = in; *p && j + 1 < len; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) {
            out[j++] = (char)tolower(c);
            prev_dash = 0;
        } else if (!prev_dash) {
            out[j++] = '-';
            prev_dash = 1;
        }
    }
    while (j > 0 && out[j - 1] == '-')  /* sondaki '-' */
        j--;
    out[j] = '\0';
}

const profile_t *profile_current(void) { return &g_current; }
const char *profile_name(void) { return g_current.name; }

void profile_set_config_dir(const char *dir)
{
    if (dir && *dir)
        snprintf(g_dir, sizeof(g_dir), "%s", dir);
}

/* <dir>/<slug>.conf dosyasindan "anahtar = deger" satirlariyla gecersiz kil.
 * Dosya yoksa sessizce gecer (gomulu degerler kalir). */
static void overlay_from_file(profile_t *p, const char *slug)
{
    char path[640];
    snprintf(path, sizeof(path), "%s/%s.conf", g_dir, slug);
    FILE *f = fopen(path, "r");
    if (!f)
        return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[128];
        /* yorum/bos satir atla */
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\n' || *s == '\0')
            continue;
        if (sscanf(s, "%63[^= \t] %*[= \t] %127s", key, val) != 2)
            continue;

        if (strcmp(key, "tier1_some_pct") == 0)        p->tier1_some_pct = atof(val);
        else if (strcmp(key, "tier2_some_pct") == 0)   p->tier2_some_pct = atof(val);
        else if (strcmp(key, "tier2_full_pct") == 0)   p->tier2_full_pct = atof(val);
        else if (strcmp(key, "reclaim_step_t1_mb") == 0) p->reclaim_step_t1 = (unsigned long long)atoll(val) * MB;
        else if (strcmp(key, "reclaim_step_t2_mb") == 0) p->reclaim_step_t2 = (unsigned long long)atoll(val) * MB;
        else if (strcmp(key, "thaw_max_frozen_sec") == 0) p->thaw_max_frozen_sec = atoi(val);
    }
    fclose(f);
    LOGI("Profil dosyasi uygulandi: %s", path);
}

int profile_apply_named(const char *name)
{
    if (!name || !*name)
        return -1;

    char slug[64];
    slugify(name, slug, sizeof(slug));

    for (int i = 0; i < N_BUILTIN; i++) {
        char bslug[64];
        slugify(k_builtin[i].name, bslug, sizeof(bslug));
        if (strcmp(slug, bslug) == 0) {
            g_current = k_builtin[i];         /* gomulu temel */
            overlay_from_file(&g_current, slug); /* varsa dosyadan gecersiz kil */
            return 0;
        }
    }
    return -1;  /* bilinmeyen profil */
}
