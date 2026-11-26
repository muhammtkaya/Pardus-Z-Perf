/* Etkileşim yoklayıcısı (M1, M3). Sorumlu: Furkan. Gereksinim: G-40.
 *
 * Fikir: her 100 ms'de uyanıp kendi belleğinin bir kısmına dokunan bir süreç,
 * bellek baskısı altında bu işi zamanında yapamaz (sayfaları takastan geri gelir,
 * direct reclaim'de bekler). Planlanan uyanma ile gerçek bitiş arasındaki fark,
 * kullanıcının hissedeceği takılmanın bir ölçüsüdür (docs/07 bölüm 5.3).
 *
 * Kullanım: probe [-m MB] [-o çıktı.csv]
 * Çıktı:    t_wall,gecikme_ms   (yalnızca eşiği aşanlar; ayrıca her 10 sn'de bir satır)
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PERIOD_NS    (100L * 1000 * 1000)   /* 100 ms */
#define THRESHOLD_MS 200.0                  /* M1: bunu aşan gecikmeler toplanır */

static volatile sig_atomic_t stop;
static void on_signal(int sig) { (void) sig; stop = 1; }

static double ts_ms(const struct timespec *t)
{
    return (double) t->tv_sec * 1e3 + (double) t->tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
    size_t mb = 64;
    const char *out = "probe.csv";
    int opt;
    while ((opt = getopt(argc, argv, "m:o:")) != -1) {
        if (opt == 'm')
            mb = (size_t) atol(optarg);
        else if (opt == 'o')
            out = optarg;
        else {
            fprintf(stderr, "Kullanım: %s [-m MB] [-o çıktı.csv]\n", argv[0]);
            return 2;
        }
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* TODO(G-40): mb kadar bellek ayır ve doldur (sıfır sayfalar zram'de çok iyi
     * sıkışır; gerçekçi olsun diye yarısını rastgele bayt yapın). */
    (void) mb;

    FILE *f = fopen(out, "w");
    if (!f) {
        perror(out);
        return 1;
    }
    fprintf(f, "t_wall,gecikme_ms\n");

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    while (!stop) {
        next.tv_nsec += PERIOD_NS;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_sec++;
            next.tv_nsec -= 1000000000L;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

        /* TODO(G-40): belleğin bir bölümüne dokun (her 4096 baytta bir yazma).
         * Her turda farklı bir bölüm: tüm bellek ~2 saniyede bir dolaşılsın. */

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double late = ts_ms(&now) - ts_ms(&next);
        /* TODO(G-40): late > THRESHOLD_MS ise CSV'ye yaz (t_wall = CLOCK_REALTIME).
         * Uzun bir takılmada birden fazla periyot kaçmışsa next'i şimdiye taşıyın,
         * yoksa döngü kaçırılan periyotları art arda çalıştırır. */
        (void) late;
    }
    fclose(f);
    return 0;
}
