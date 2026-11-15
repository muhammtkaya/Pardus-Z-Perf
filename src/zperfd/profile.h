/* SPDX-License-Identifier: GPL-3.0-or-later
 * profile.h: Calisma zamani ayarlanabilir politika profilleri.
 *
 * Profil, politika motorunun ve kademe siniflandirmasinin kullandigi
 * ayarlanabilir degerleri (PSI esikleri, reclaim adim boyutlari, donmus
 * uygulamanin periyodik cozulme suresi) tek bir yerde toplar. Uc gomulu
 * profil vardir (Dengeli / Ofis / Akilli Tahta); ayrica kurulumda
 * /etc/zperf/profiles/<slug>.conf dosyasi varsa degerleri gecersiz kilar.
 *
 * Arayuz "profile" IPC komutuyla profil degistirir; dosya yoksa gomulu
 * varsayilanlar kullanildigi icin sistem dosyasiz da calisir.
 */
#ifndef ZPERF_PROFILE_H
#define ZPERF_PROFILE_H

typedef struct {
    char   name[64];              /* gosterim adi (ornek: "Dengeli") */
    double tier1_some_pct;        /* kademe 1 esigi: some avg10 >= bu     */
    double tier2_some_pct;        /* kademe 2 esigi: some avg10 >= bu     */
    double tier2_full_pct;        /* kademe 2 esigi: full avg10 >= bu     */
    unsigned long long reclaim_step_t1;  /* kademe 1 reclaim adimi (bayt) */
    unsigned long long reclaim_step_t2;  /* kademe 2 reclaim adimi (bayt) */
    int    thaw_max_frozen_sec;   /* bu kadar sn donmus kalan uygulama coz */
} profile_t;

/* Gecerli profil (baslangicta "Dengeli"). Asla NULL donmez. */
const profile_t *profile_current(void);

/* Gecerli profilin gosterim adi. */
const char *profile_name(void);

/* Ada gore profil uygular: once gomulu varsayilani yukler, sonra varsa
 * <config_dir>/<slug>.conf dosyasindan alanlari gecersiz kilar.
 * Ad, bosluk/buyuk-kucuk harf fark etmeksizin eslenir ("Akilli Tahta",
 * "akilli-tahta" ayni). Basari 0, bilinmeyen profil -1. */
int profile_apply_named(const char *name);

/* .conf dosyalarinin arama dizini (varsayilan: /etc/zperf/profiles).
 * Gelistirmede "config/profiles" gibi goreli bir yola ayarlanabilir. */
void profile_set_config_dir(const char *dir);

#endif /* ZPERF_PROFILE_H */
