/* Küçük test yardımcısı: harici çatı yok, `make test` ile çalışır. */
#pragma once

#include <stdio.h>

static int t_fails, t_checks;

#define CHECK(cond)                                                              \
    do {                                                                         \
        t_checks++;                                                              \
        if (!(cond)) {                                                           \
            t_fails++;                                                           \
            fprintf(stderr, "  BAŞARISIZ %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

#define RUN(test)                                \
    do {                                         \
        int before = t_fails;                    \
        test();                                  \
        printf("%-6s %s\n", t_fails == before ? "GEÇTİ" : "KALDI", #test); \
    } while (0)

#define SUMMARY() \
    (printf("%d kontrol, %d başarısız\n", t_checks, t_fails), t_fails ? 1 : 0)
