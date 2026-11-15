/* SPDX-License-Identifier: GPL-3.0-or-later
 * applist.h: Servisin yonettigi uygulamalarin kaydi.
 *
 * Her uygulama bir ad, kok PID, en son odak zamani (LRU icin) ve durum
 * bayraklari tutar. Politika motoru, kademe 2'de "en uzun suredir odak
 * almamis" (LRU) uygulamayi buradan secer.
 */
#ifndef ZPERF_APPLIST_H
#define ZPERF_APPLIST_H

#include <sys/types.h>

#define APP_NAME_MAX 64
#define APP_MAX      64

typedef struct {
    char  name[APP_NAME_MAX];
    pid_t pid;             /* kok surec */
    long  last_focus_ms;   /* en son odak aldigi (veya biraktigi) an */
    int   frozen;          /* 1 = su an donmus */
    int   never_freeze;    /* 1 = "asla dondurma" listesinde */
    int   managed;         /* 1 = cgroup'a tasindi */
    int   used;            /* dahili: yuva dolu mu */
} app_t;

void   applist_init(void);
app_t *applist_get(const char *name);               /* yoksa NULL */
app_t *applist_add(const char *name, pid_t pid);    /* dolu ise NULL */
void   applist_mark_focus(const char *name);        /* last_focus = simdi */
void   applist_set_never_freeze(const char *name, int on);

/* Kurban: odakta olmayan, "asla dondurma" olmayan, henuz donmamis
 * uygulamalar arasinda en eski last_focus'a sahip olani dondurur. */
app_t *applist_pick_victim(const char *focused_name);

int    applist_count(void);        /* dolu yuva sayisi degil, tarama ust siniri */
app_t *applist_at(int i);          /* i. yuva (kullanilmiyorsa used=0) */

#endif /* ZPERF_APPLIST_H */
