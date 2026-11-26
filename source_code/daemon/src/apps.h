/* Yönetilen uygulamaların tablosu. Sabit boyutludur: servis bellek baskısı anında
 * da çalışmak zorunda olduğu için döngüde malloc yapılmaz (docs/06 bölüm 4). */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define ZP_MAX_APPS 128

struct zp_app {
    pid_t    pid;               /* pencerenin sahibi (kök) süreç */
    uid_t    uid;
    char     comm[32];
    char     unit[96];          /* "zperf-<comm>-<pid>.scope"; boşsa henüz gruplanmadı */
    char     cgroup[256];       /* "/user.slice/user-1000.slice/zperf-....scope" */
    double   last_focus;        /* util_now(); boşta kalma süresi buradan */
    uint64_t mem, swap;         /* son okunan memory.current / memory.swap.current */
    bool     frozen;
    bool     never_freeze;      /* kullanıcı listesi */
    bool     excluded;          /* sistem bileşeni (docs/05 bölüm 7) */
    bool     audio;             /* ajan "ses çalıyor" dedi */
};

struct zp_apps {
    struct zp_app a[ZP_MAX_APPS];
    size_t        n;
};

struct zp_app *apps_find_pid(struct zp_apps *t, pid_t pid);
struct zp_app *apps_add(struct zp_apps *t, pid_t pid, uid_t uid, const char *comm);
void           apps_remove(struct zp_apps *t, struct zp_app *app);

/* comm ve pid'den güvenli scope adı üretir (U-09): yalnızca [a-z0-9_-],
 * comm kısmı en fazla 32 karakter. "soffice.bin" → "zperf-soffice-bin-3120.scope" */
int apps_unit_name(const char *comm, pid_t pid, char *out, size_t n);

/* Dondurulmaması gereken sistem bileşeni mi? (docs/05 bölüm 7) */
bool apps_is_system_component(const char *comm);
