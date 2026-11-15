/* SPDX-License-Identifier: GPL-3.0-or-later
 * cgroup.h: cgroup v2 ile uygulama ayirma, dondurma ve bellek geri kazanimi.
 *
 * Pardus 25.0 XFCE'de tum uygulamalar oturumun tek cgroup'unda calisir; tek
 * bir uygulamayi dondurmak icin once onu ayirmak gerekir. Bu modul, kendi
 * olusturdugu /sys/fs/cgroup/zperf.slice alt agacinda her uygulama icin bir
 * cgroup acar, surec agacini oraya tasir, cgroup.freeze ile dondurur ve
 * memory.reclaim ile soguk sayfalari zram'e iter.
 *
 * GUVENLIK: Servis cokse/kapansa bile donmus uygulama kalmamalidir; bu yuzden
 * cikista cgroup_cleanup_all() TUM alt cgroup'lari cozer.
 */
#ifndef ZPERF_CGROUP_H
#define ZPERF_CGROUP_H

#include <sys/types.h>

/* cgroup v2'yi dogrular, zperf.slice tabanini olusturur ve (mumkunse)
 * memory denetleyicisini alt agac icin etkinlestirir. 0 basarili, -1 hata. */
int cgroup_init(void);

/* Bir uygulama cgroup'u olusturur (zperf.slice/app-<name>). Zaten varsa 0. */
int cgroup_app_create(const char *name);

/* Tek bir sureci uygulama cgroup'una tasir. */
int cgroup_app_move_pid(const char *name, pid_t pid);

/* root sureci ve tum alt soyunu (/proc taranarak) cgroup'a tasir.
 * Cok surecli uygulamalar (tarayicilar) icin. Tasinan surec sayisini doner. */
int cgroup_app_move_tree(const char *name, pid_t root);

/* Uygulama cgroup'unu dondurur (on=1) veya cozer (on=0). */
int cgroup_app_freeze(const char *name, int on);

/* cgroup.events icindeki "frozen 1" durumunu okur: 1 donmus, 0 degil, -1 hata. */
int cgroup_is_frozen(const char *name);

/* memory.current (su an kullanilan bellek, bayt); -1 hata. */
long long cgroup_app_mem_current(const char *name);

/* memory.reclaim'e <bytes> yazarak soguk sayfalari geri kazandirir. */
int cgroup_app_reclaim(const char *name, unsigned long long bytes);

/* Uygulamayi cozer, sureclerini disari tasir ve cgroup'u siler. */
int cgroup_app_destroy(const char *name);

/* zperf.slice altindaki TUM app-* cgroup'larini cozer ve temizler.
 * Kapanista/cokuste cagrilir; geriye donmus uygulama birakmaz. */
void cgroup_cleanup_all(void);

#endif /* ZPERF_CGROUP_H */
