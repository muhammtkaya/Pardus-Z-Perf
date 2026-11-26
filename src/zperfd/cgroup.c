/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cgroup.h"
#include "util.h"
#include "log.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CG_ROOT "/sys/fs/cgroup"
#define CG_BASE CG_ROOT "/zperf.slice"

/* app-<name> icin tam sysfs yolunu uretir. */
static void app_path(char *buf, size_t len, const char *name, const char *attr)
{
    if (attr)
        snprintf(buf, len, CG_BASE "/app-%s/%s", name, attr);
    else
        snprintf(buf, len, CG_BASE "/app-%s", name);
}

int cgroup_init(void)
{
    /* cgroup v2 mi? (birlesik hiyerarsi) */
    if (!util_exists(CG_ROOT "/cgroup.controllers")) {
        LOGE("cgroup v2 bulunamadi (%s yok).", CG_ROOT "/cgroup.controllers");
        return -1;
    }

    if (mkdir(CG_BASE, 0755) != 0 && errno != EEXIST) {
        LOGE("%s olusturulamadi: %s", CG_BASE, strerror(errno));
        return -1;
    }

    /* Alt uygulamalarda memory.* dosyalarinin (reclaim/current) olmasi icin
     * zperf.slice'in alt agac denetimine memory eklenmeli. Freezer ise cekirdek
     * cekirdegindedir, denetleyici gerektirmez; bu adim basarisiz olsa da
     * dondurma calismaya devam eder. */
    if (util_write_str(CG_BASE "/cgroup.subtree_control", "+memory") != 0)
        LOGW("memory denetleyicisi etkinlestirilemedi (%s); "
             "dondurma calisir, memory.reclaim calismayabilir.",
             strerror(errno));

    LOGI("cgroup tabani hazir: %s", CG_BASE);
    return 0;
}

int cgroup_app_create(const char *name)
{
    char path[256];
    app_path(path, sizeof(path), name, NULL);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        LOGE("%s olusturulamadi: %s", path, strerror(errno));
        return -1;
    }
    return 0;
}

int cgroup_app_move_pid(const char *name, pid_t pid)
{
    char path[256], val[32];
    app_path(path, sizeof(path), name, "cgroup.procs");
    snprintf(val, sizeof(val), "%d", (int)pid);
    if (util_write_str(path, val) != 0) {
        LOGW("PID %d, app-%s'e tasinamadi: %s", (int)pid, name, strerror(errno));
        return -1;
    }
    return 0;
}

/* /proc/<pid>/stat icinden ebeveyn PID'i okur; hata halinde -1. */
static pid_t parent_of(pid_t pid)
{
    char path[64], buf[512];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    if (util_read_str(path, buf, sizeof(buf)) < 0)
        return -1;
    /* Bicim: pid (comm) state ppid ...  comm parantez icerebilir, sondan ara. */
    char *rp = strrchr(buf, ')');
    if (!rp)
        return -1;
    int state;
    int ppid = -1;
    if (sscanf(rp + 1, " %c %d", (char *)&state, &ppid) < 2)
        return -1;
    return (pid_t)ppid;
}

int cgroup_app_move_tree(const char *name, pid_t root)
{
    int moved = 0;
    /* KRITIK: kendi surecimizi (zperfd) asla bir uygulama cgroup'una tasima.
     * Aksi halde, zperfd odaktaki uygulamanin bir alt sureci oldugunda (or.
     * test o uygulamanin terminalinden baslatildiginda) bu cgroup dondurulunca
     * SERVIS KENDINI DONDURUR ve tamamen kilitlenir (test bulgusu). */
    pid_t self = getpid();
    if (root == self)
        return 0;

    /* Once koku tasi. */
    if (cgroup_app_move_pid(name, root) == 0)
        moved++;

    /* /proc'u tarayip bu kokun soyundan gelen tum surecleri tasi.
     * Basit yaklasim: her surecin ebeveynini root'a kadar izle. */
    DIR *d = opendir("/proc");
    if (!d)
        return moved;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        char *end;
        long p = strtol(e->d_name, &end, 10);
        if (*end != '\0' || p <= 0 || (pid_t)p == root || (pid_t)p == self)
            continue;              /* kendi surecimizi tasima (bkz. yukarisi) */

        /* p'nin ata zincirinde root var mi? */
        pid_t cur = (pid_t)p;
        int depth = 0;
        int is_desc = 0;
        while (cur > 1 && depth++ < 64) {
            pid_t pp = parent_of(cur);
            if (pp == root) { is_desc = 1; break; }
            if (pp <= 1) break;
            cur = pp;
        }
        if (is_desc && cgroup_app_move_pid(name, (pid_t)p) == 0)
            moved++;
    }
    closedir(d);
    return moved;
}

int cgroup_app_freeze(const char *name, int on)
{
    char path[256];
    app_path(path, sizeof(path), name, "cgroup.freeze");
    if (util_write_str(path, on ? "1" : "0") != 0) {
        LOGW("app-%s %s islemi basarisiz: %s", name,
             on ? "dondurma" : "cozme", strerror(errno));
        return -1;
    }
    LOGI("app-%s %s", name, on ? "donduruldu" : "cozuldu");
    return 0;
}

int cgroup_is_frozen(const char *name)
{
    char path[256], buf[256];
    app_path(path, sizeof(path), name, "cgroup.events");
    if (util_read_str(path, buf, sizeof(buf)) < 0)
        return -1;
    char *p = strstr(buf, "frozen");
    if (!p)
        return -1;
    int v = 0;
    if (sscanf(p + strlen("frozen"), " %d", &v) != 1)
        return -1;
    return v;
}

long long cgroup_app_mem_current(const char *name)
{
    char path[256], buf[64];
    app_path(path, sizeof(path), name, "memory.current");
    if (util_read_str(path, buf, sizeof(buf)) < 0)
        return -1;
    long long v = -1;
    if (sscanf(buf, "%lld", &v) != 1)
        return -1;
    return v;
}

int cgroup_app_reclaim(const char *name, unsigned long long bytes)
{
    char path[256], val[32];
    app_path(path, sizeof(path), name, "memory.reclaim");
    snprintf(val, sizeof(val), "%llu", bytes);
    if (util_write_str(path, val) != 0) {
        /* EAGAIN: cekirdek istenen miktarin TAMAMINI geri kazanamadi ama
         * kismen basarmis olabilir (testle dogrulandi). Bu bir hata degil;
         * politika zaten kucuk adimlarla ilerledigi icin normaldir. */
        if (errno == EAGAIN) {
            LOGD("app-%s: reclaim kismi (istenen %llu bayt tam karsilanmadi).",
                 name, bytes);
            return 0;
        }
        LOGW("app-%s icin memory.reclaim basarisiz: %s", name, strerror(errno));
        return -1;
    }
    return 0;
}

/* Bir cgroup'taki tum surecleri root cgroup'a geri tasir (en iyi caba). */
static void move_procs_to_root(const char *procs_path)
{
    char buf[8192];
    if (util_read_str(procs_path, buf, sizeof(buf)) < 0)
        return;
    char *save = NULL;
    for (char *tok = strtok_r(buf, "\n", &save); tok;
         tok = strtok_r(NULL, "\n", &save)) {
        if (*tok)
            (void)util_write_str(CG_ROOT "/cgroup.procs", tok);
    }
}

int cgroup_app_destroy(const char *name)
{
    char dir[256], procs[256];
    app_path(dir, sizeof(dir), name, NULL);
    app_path(procs, sizeof(procs), name, "cgroup.procs");

    (void)cgroup_app_freeze(name, 0);      /* once coz */
    move_procs_to_root(procs);             /* sureckleri disari al */

    if (rmdir(dir) != 0 && errno != ENOENT) {
        LOGW("%s silinemedi: %s", dir, strerror(errno));
        return -1;
    }
    return 0;
}

void cgroup_cleanup_all(void)
{
    DIR *d = opendir(CG_BASE);
    if (!d)
        return;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "app-", 4) != 0)
            continue;
        const char *name = e->d_name + 4;  /* "app-" sonrasi */
        /* GUVENLIK onceligi: her kosulda COZ. */
        cgroup_app_freeze(name, 0);
        cgroup_app_destroy(name);
    }
    closedir(d);
    LOGI("Tum uygulama cgroup'lari cozuldu ve temizlendi.");
}
