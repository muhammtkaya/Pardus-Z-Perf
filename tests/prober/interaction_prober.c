/* SPDX-License-Identifier: GPL-3.0-or-later
 * interaction_prober — M1 (etkilesim takilmasi) olceri.
 *
 * Fikir: On plandaki uygulamanin tepkiselligini taklit eder. Sabit bir
 * periyotla (ornek 50 ms) uyanmasi gereken bir dongu kurar (CLOCK_MONOTONIC,
 * TIMER_ABSTIME). Her uyanista, HEDEFLENEN uyanma ani ile GERCEK an arasindaki
 * gecikme = "takilma" (stall). Sistem bellek baskisi altinda thrash ederken
 * zamanlayici gecikmeleri ve major page-fault'lar bu gecikmeyi buyutur; iyi bir
 * politika bunu kucuk tutmalidir.
 *
 * Ek olarak, --worksize ile bir calisma kumesi (working set) ayrilir ve her
 * tikta bir kismi yazilir; boylece prober gercek bir on plan uygulamasi gibi
 * bellekte kalmaya calisir (reclaim kurbani OLMAMALI — odak korumasini sinar).
 *
 * Cikti: CSV (t_ms,stall_ms) --out dosyasina; ozet (p50/p95/p99/max) stderr'e.
 * Root GEREKMEZ. Olcum senaryolarinda on planda calistirilir (bkz. tests/measure).
 */
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static int cmp_long(const void *a, const void *b)
{
    long x = *(const long *)a, y = *(const long *)b;
    return (x > y) - (x < y);
}

static double pct(long *sorted, int n, double p)
{
    if (n <= 0) return 0.0;
    double idx = p / 100.0 * (n - 1);
    int lo = (int)idx;
    if (lo >= n - 1) return (double)sorted[n - 1];
    double frac = idx - lo;
    return sorted[lo] * (1.0 - frac) + sorted[lo + 1] * frac;
}

int main(int argc, char **argv)
{
    int    duration_sec = 30;
    long   interval_ms  = 50;
    long   worksize_mb  = 64;
    const char *out_path = NULL;

    static const struct option opts[] = {
        { "duration", required_argument, 0, 'd' },
        { "interval", required_argument, 0, 'i' },
        { "worksize", required_argument, 0, 'w' },
        { "out",      required_argument, 0, 'o' },
        { "help",     no_argument,       0, 'h' },
        { 0, 0, 0, 0 },
    };
    int c;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (c) {
        case 'd': duration_sec = atoi(optarg); break;
        case 'i': interval_ms  = atol(optarg); break;
        case 'w': worksize_mb  = atol(optarg); break;
        case 'o': out_path     = optarg; break;
        case 'h':
        default:
            fprintf(stderr,
                "Kullanim: %s [--duration SN] [--interval MS] "
                "[--worksize MB] [--out dosya.csv]\n", argv[0]);
            return (c == 'h') ? 0 : 1;
        }
    }
    if (interval_ms < 1) interval_ms = 1;

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    /* Calisma kumesi: ayir ve ilk dokunusla resident yap. */
    size_t ws = (size_t)worksize_mb * 1024 * 1024;
    unsigned char *buf = NULL;
    if (ws > 0) {
        buf = malloc(ws);
        if (!buf) {
            fprintf(stderr, "uyari: %ld MB calisma kumesi ayrilamadi\n", worksize_mb);
            ws = 0;
        } else {
            memset(buf, 1, ws);
        }
    }

    long cap = (long)duration_sec * 1000 / interval_ms + 16;
    long *stalls = malloc((size_t)cap * sizeof(long));
    if (!stalls) { fprintf(stderr, "bellek yok\n"); free(buf); return 1; }
    long n = 0;

    FILE *out = NULL;
    if (out_path) {
        out = fopen(out_path, "w");
        if (out) fprintf(out, "t_ms,stall_ms\n");
        else fprintf(stderr, "uyari: %s acilamadi: %s\n", out_path, strerror(errno));
    }

    struct timespec start, next;
    clock_gettime(CLOCK_MONOTONIC, &start);
    next = start;
    long interval_ns = interval_ms * 1000000L;
    size_t touch_off = 0;

    while (!g_stop) {
        /* Bir sonraki hedef uyanma ani. */
        next.tv_nsec += interval_ns;
        while (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }

        int rc;
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        } while (rc == EINTR && !g_stop);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        /* Hedeflenen ile gercek an farki (ms) = takilma. */
        double stall_ms = (now.tv_sec - next.tv_sec) * 1000.0
                        + (now.tv_nsec - next.tv_nsec) / 1e6;
        if (stall_ms < 0) stall_ms = 0;

        /* Calisma kumesine biraz dokun (on plan uygulamasi gibi resident kal). */
        if (buf && ws) {
            size_t chunk = 1024 * 1024;  /* her tik 1 MB yaz */
            for (size_t k = 0; k < chunk; k += 4096) {
                buf[(touch_off + k) % ws] = (unsigned char)(touch_off + k);
            }
            touch_off = (touch_off + chunk) % ws;
        }

        double t_ms = (now.tv_sec - start.tv_sec) * 1000.0
                    + (now.tv_nsec - start.tv_nsec) / 1e6;
        long sm = (long)(stall_ms + 0.5);
        if (n < cap) stalls[n++] = sm;
        if (out) fprintf(out, "%.0f,%ld\n", t_ms, sm);

        if (t_ms >= (double)duration_sec * 1000.0) break;
    }

    if (out) fclose(out);

    /* Ozet. */
    qsort(stalls, (size_t)n, sizeof(long), cmp_long);
    double p50 = pct(stalls, (int)n, 50);
    double p95 = pct(stalls, (int)n, 95);
    double p99 = pct(stalls, (int)n, 99);
    long   mx  = n ? stalls[n - 1] : 0;
    double mean = 0;
    for (long i = 0; i < n; i++) mean += stalls[i];
    mean = n ? mean / n : 0;

    fprintf(stderr,
        "M1 takilma ozeti: n=%ld  ort=%.1f  p50=%.0f  p95=%.0f  p99=%.0f  max=%ld (ms)\n",
        n, mean, p50, p95, p99, mx);

    free(stalls);
    free(buf);
    return 0;
}
