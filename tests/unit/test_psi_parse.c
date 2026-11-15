/* SPDX-License-Identifier: GPL-3.0-or-later
 * psi_avg10() ayristiricisinin birim testi (gercek cekirdek gerekmez).
 * Derleme:
 *   gcc -I src/zperfd tests/unit/test_psi_parse.c \
 *       src/zperfd/psi.c src/zperfd/util.c src/zperfd/log.c -o /tmp/test_psi
 */
#include "psi.h"
#include <stdio.h>

static int near(double a, double b) { double d = a - b; return d < 0.01 && d > -0.01; }

int main(void)
{
    const char *sample =
        "some avg10=23.45 avg60=10.00 avg300=1.00 total=123456\n"
        "full avg10=6.78 avg60=2.00 avg300=0.50 total=45678\n";

    double some = psi_avg10(sample, PSI_SOME);
    double full = psi_avg10(sample, PSI_FULL);
    printf("some avg10 = %.2f (beklenen 23.45)\n", some);
    printf("full avg10 = %.2f (beklenen 6.78)\n", full);

    int fails = 0;
    if (!near(some, 23.45)) { printf("FAIL: some yanlis\n"); fails++; }
    if (!near(full, 6.78))  { printf("FAIL: full yanlis\n"); fails++; }

    /* Eksik/bozuk girdi -1 dondurmeli */
    if (psi_avg10("garbage", PSI_SOME) != -1.0) { printf("FAIL: bozuk girdi\n"); fails++; }

    printf(fails == 0 ? "PASS\n" : "BASARISIZ (%d)\n", fails);
    return fails == 0 ? 0 : 1;
}
