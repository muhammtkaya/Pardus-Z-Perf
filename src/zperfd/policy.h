/* SPDX-License-Identifier: GPL-3.0-or-later
 * policy.h: Baski kademesi + odak bilgisini uygulama eylemlerine baglar.
 *
 *   Kademe 1: odakta olmayan yonetilen uygulamalara kucuk adimli reclaim.
 *   Kademe 2: en uzun suredir odak almamis kurbani dondurup sikistirir.
 *   Odak degisimi: yeni odaktaki uygulama donmussa hemen cozulur.
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

/* Su an odakta olan uygulamanin adi ("" ise yok). */
const char *policy_focused(void);

#endif /* ZPERF_POLICY_H */
