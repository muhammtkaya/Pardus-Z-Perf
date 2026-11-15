/* SPDX-License-Identifier: GPL-3.0-or-later
 * applist kurban secimi (LRU) birim testi. Cekirdek gerekmez.
 * Derleme:
 *   gcc -I src/zperfd tests/unit/test_applist.c \
 *       src/zperfd/applist.c src/zperfd/util.c -o /tmp/test_applist
 */
#include "applist.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, m) do { if (c) printf("  [GECTI] %s\n", m); \
    else { printf("  [KALDI] %s\n", m); fails++; } } while (0)

int main(void)
{
    applist_init();

    app_t *a = applist_add("appA", 101);
    usleep(10000);
    app_t *b = applist_add("appB", 102);
    usleep(10000);
    app_t *c = applist_add("appC", 103);
    (void)a; (void)b; (void)c;

    CHECK(applist_get("appB") != NULL, "eklenen uygulama bulunuyor");
    CHECK(applist_get("yok") == NULL, "olmayan uygulama NULL");

    /* Eklenme sirasi A,B,C; en eski odak A. appC odakta.
     * Kurban, odakta olmayanlar (A,B) arasindan en eski = A olmali. */
    app_t *v = applist_pick_victim("appC");
    CHECK(v && strcmp(v->name, "appA") == 0, "kurban en eski (appA)");

    /* appA tekrar odak alirsa artik en eski appB olur. */
    applist_mark_focus("appA");
    v = applist_pick_victim("appC");
    CHECK(v && strcmp(v->name, "appB") == 0, "appA tazelendi -> kurban appB");

    /* "asla dondurma" isaretli uygulama kurban secilemez. */
    applist_set_never_freeze("appB", 1);
    v = applist_pick_victim("appC");
    CHECK(v && strcmp(v->name, "appA") == 0, "never_freeze appB atlandi -> appA");

    /* Donmus uygulama tekrar kurban olmaz. */
    applist_get("appA")->frozen = 1;
    v = applist_pick_victim("appC");
    CHECK(v == NULL, "kalan aday yok (appB korumali, appA donmus)");

    printf("\n%s\n", fails == 0 ? "TUM TESTLER GECTI" : "BAZI TESTLER KALDI");
    return fails ? 1 : 0;
}
