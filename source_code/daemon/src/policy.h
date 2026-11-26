/* Kademeli bellek politikası: saf durum makinesi (docs/05 bölüm 4).
 * policy_step() dosya okumaz, D-Bus çağırmaz, saati kendisi almaz; tüm girdiler
 * parametredir. Bu sayede daemon/tests/test_policy.c ile çekirdeğe dokunmadan
 * test edilebilir. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "apps.h"
#include "config.h"
#include "psi.h"

#define ZP_FREEZE_INTERVAL_S 5.0    /* iki dondurma arasında en az (G-06) */
#define ZP_HYSTERESIS_S      10.0   /* seviye düşürmeden önce bekleme (G-10) */

struct policy_state {
    int    level;           /* 0 normal, 1 proaktif sıkıştırma, 2 dondur ve sıkıştır */
    double calm_since;      /* baskının çıkış eşiğinin altına indiği an; 0 = inmedi */
    double last_freeze;     /* son ACT_FREEZE zamanı */
};

struct policy_input {
    int                   k1_fired;     /* bu adımda K1 tetikleyicisi ateşlendi mi */
    int                   k2_fired;     /* K2 (some veya full) */
    struct psi_avgs       psi;
    double                now;          /* util_now() */
    const struct zp_app  *apps;
    size_t                napps;
    int                   focused_idx;  /* apps içindeki odaktaki uygulama; yoksa -1 */
    int                   enabled;      /* politika kapalıysa hiçbir eylem üretilmez */
};

enum policy_act_kind { ACT_RECLAIM, ACT_FREEZE };

struct policy_action {
    enum policy_act_kind kind;
    int                  app_idx;       /* apps dizisindeki indis */
    uint64_t             bytes;         /* ACT_RECLAIM için */
};

void policy_init(struct policy_state *s);

/* Bir adım işletir; en fazla max_out eylem yazar ve eylem sayısını döndürür. */
int policy_step(struct policy_state *s, const struct zp_profile *p,
                const struct policy_input *in,
                struct policy_action *out, size_t max_out);
