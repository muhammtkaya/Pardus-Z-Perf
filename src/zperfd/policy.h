/* SPDX-License-Identifier: GPL-3.0-or-later
 * policy.h: Baski kademesi + odak bilgisini uygulama eylemlerine baglar.
 *
 *   Kademe 1: odakta olmayan yonetilen uygulamalara kucuk adimli reclaim.
 *   Kademe 2: o an EN COK BELLEK TUKETEN uygun uygulamayi dondurup sikistirir.
 *   Odak degisimi: yeni odaktaki uygulama donmussa hemen cozulur.
 *   Periyodik bakim: cok uzun donmus uygulamayi kisa sure cozer.
 *
 * Esikler/adim boyutlari gecerli profilden (profile.h) okunur.
 */
#ifndef ZPERF_POLICY_H
#define ZPERF_POLICY_H

#include <sys/types.h>

void policy_init(void);

/* Ajandan odak bildirimi geldiginde cagrilir: uygulamayi (gerekirse)
 * yonetime alir, cgroup'a tasir, donmussa cozer ve odak zamanini gunceller. */
void policy_on_focus(const char *name, pid_t pid);

/* Her baski degerlendirmesinde (PSI olayi veya periyodik) cagrilir.
 * Verilen kademeye gore reclaim/freeze uygular. Yapilan eylem sayisini doner. */
int policy_on_tier(int tier);

/* Periyodik (durum timer'i) bakim: profile.thaw_max_frozen_sec'ten uzun
 * suredir donmus uygulamalari kisa sure cozer (ag/ses zaman asimi yan etkisi). */
void policy_maintenance(void);

/* Arayuzden gelen "thaw" komutu: adiyla verilen donmus uygulamayi hemen cozer
 * (odakta olmasa bile). Donmus degilse veya yoksa hicbir sey yapmaz. */
void policy_thaw(const char *name);

/* "Asla dondurma" bayragini ayarlar. on=1 ve uygulama su an donuksa ayrica
 * hemen cozulur. on=0 iken uygulama tekrar normal kurban adayi olur. */
void policy_set_never_freeze(const char *name, int on);

/* Yonetimi ac/kapat. Kapatilinca tum donmus uygulamalar cozulur ve
 * policy_on_tier hicbir eylem yapmaz (arayuz anahtari icin). */
void policy_set_enabled(int on);
int  policy_enabled(void);

/* Su an odakta olan uygulamanin adi ("" ise yok). */
const char *policy_focused(void);

/* Olay bildirimi geri cagrimi: dondurma/cozme olunca cagrilir
 * (ev = "frozen"|"thawed"). main.c bunu IPC yayinina baglar; testlerde NULL. */
void policy_set_event_cb(void (*fn)(const char *ev, const char *name));

/* Ses calma sorgusu geri cagrimi: kurban secerken ses calan uygulamayi
 * atlamak icin kullanilir. NULL ise atlama yapilmaz (saf kurban secimi). */
void policy_set_audio_cb(int (*fn)(pid_t pid));

/* Asla dondurulmayacak uygulama adlari (bosluk/virgulle ayrilmis liste; genelde
 * ZPERF_PROTECT ortam degiskeninden). Testi suruk uygulamanin (VSCode/terminal)
 * dondurulmasini onlemek icin kullanilir. NULL/"" = koruma yok. */
void policy_set_protected(const char *csv);

#endif /* ZPERF_POLICY_H */
