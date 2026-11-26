/* Sorumlu: Furkan. Gereksinimler: G-05, G-06, G-09, G-10, G-13. Karar: TK-04.
 * Davranışın tanımı: docs/05 bölüm 3.3 ve 4. Testler: daemon/tests/test_policy.c
 * (U-01 – U-07). Testler önce yazıldı; bu fonksiyon yazılana kadar kalırlar. */
#include "policy.h"

#include <string.h>

void policy_init(struct policy_state *s)
{
    memset(s, 0, sizeof *s);
}

int policy_step(struct policy_state *s, const struct zp_profile *p,
                const struct policy_input *in,
                struct policy_action *out, size_t max_out)
{
    (void) s; (void) p; (void) in; (void) out; (void) max_out;
    /* TODO(G-05, G-06, G-10):
     *
     * 1. Seviye geçişleri
     *    - in->enabled == 0 ise: seviye 0, eylem yok.
     *    - k2_fired → seviye 2; k1_fired → en az seviye 1.
     *    - İniş (histerezis): seviye 2'de full10 < 1 ve some10 < k1 eşiği,
     *      seviye 1'de some10 < p->exit_some10 koşulu ZP_HYSTERESIS_S boyunca
     *      sürerse bir seviye in (calm_since ile).
     *
     * 2. Seviye >= 1: her aday uygulama için bir ACT_RECLAIM
     *    aday = odakta değil, excluded değil, never_freeze olsa da OLUR (sıkıştırma
     *    dondurma değildir), unit[0] != 0 (gruplanmış), frozen değil.
     *    bytes = min(p->reclaim_step, app.mem / 10)
     *
     * 3. Seviye == 2 ve p->freeze ve son dondurmadan ZP_FREEZE_INTERVAL_S geçtiyse:
     *    kurban = odakta değil, excluded/never_freeze/audio/frozen değil,
     *             (now - last_focus) >= p->idle_s, unit[0] != 0
     *             adaylar içinde mem en büyük
     *    → bir ACT_FREEZE, last_freeze = now
     *
     * Bu fonksiyon hiçbir zaman süreç öldürmez (G-11, TK-05).
     */
    return 0;
}
