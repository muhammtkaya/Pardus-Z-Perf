/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "zram.h"
#include "util.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/swap.h>
#include <unistd.h>

#define ZRAM_CONTROL_ADD    "/sys/class/zram-control/hot_add"
#define ZRAM_CONTROL_REMOVE "/sys/class/zram-control/hot_remove"

/* zram cekirdek modulunun yuklu oldugundan emin olur. */
static int ensure_module(void)
{
    if (util_exists("/sys/class/zram-control"))
        return 0; /* zaten var */

    char *const argv[] = { "modprobe", "zram", NULL };
    if (util_run(argv) != 0) {
        LOGE("modprobe zram basarisiz");
        return -1;
    }
    return util_exists("/sys/class/zram-control") ? 0 : -1;
}

/* Yeni bir zram aygiti ekler ve numarasini dondurur; hata halinde -1. */
static int add_device(void)
{
    char buf[32];
    long n = util_read_str(ZRAM_CONTROL_ADD, buf, sizeof(buf));
    if (n < 0) {
        LOGE("hot_add okunamadi: %s", strerror(errno));
        return -1;
    }
    int id = -1;
    if (sscanf(buf, "%d", &id) != 1 || id < 0) {
        LOGE("hot_add beklenmeyen cikti: '%s'", buf);
        return -1;
    }
    return id;
}

static int sysfs_write(int id, const char *attr, const char *value)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/block/zram%d/%s", id, attr);
    if (util_write_str(path, value) != 0) {
        LOGW("%s = '%s' yazilamadi: %s", path, value, strerror(errno));
        return -1;
    }
    return 0;
}

/* comp_algorithm icinde koseli parantezli ETKIN algoritmayi okur.
 * Ornek icerik: "[lzo-rle] lzo lz4 zstd" -> "lzo-rle". */
static void read_active_algo(int id, char *out, size_t len)
{
    char path[128], buf[256];
    snprintf(path, sizeof(path), "/sys/block/zram%d/comp_algorithm", id);
    if (util_read_str(path, buf, sizeof(buf)) < 0) {
        snprintf(out, len, "bilinmiyor");
        return;
    }
    char *lb = strchr(buf, '[');
    char *rb = lb ? strchr(lb, ']') : NULL;
    if (lb && rb && rb > lb + 1) {
        size_t n = (size_t)(rb - lb - 1);
        if (n >= len) n = len - 1;
        memcpy(out, lb + 1, n);
        out[n] = '\0';
    } else {
        snprintf(out, len, "bilinmiyor");
    }
}

int zram_setup(zram_dev_t *out, const char *algorithm,
               unsigned ram_percent, int priority)
{
    memset(out, 0, sizeof(*out));
    out->device_id = -1;

    if (!algorithm || !*algorithm)
        algorithm = "lz4";
    if (ram_percent == 0)
        ram_percent = 100;

    /* Sistemde zaten etkin bir zram takasi varsa ona dokunma. */
    if (util_zram_swap_active()) {
        LOGI("Etkin bir zram takasi zaten var; mevcut aygit kullanilacak.");
        out->owned = 0;
        return 0;
    }

    uint64_t ram = util_total_ram_bytes();
    if (ram == 0) {
        LOGE("MemTotal okunamadi; zram kurulamadi.");
        return -1;
    }
    uint64_t disksize = ram / 100ULL * ram_percent;

    if (ensure_module() != 0)
        return -1;

    int id = add_device();
    if (id < 0)
        return -1;

    /* Onemli: comp_algorithm, disksize'dan ONCE ayarlanmali.
     * Cekirdek istenen algoritmayi desteklemiyorsa yazim EINVAL verir;
     * bu olumcul degildir, cekirdek varsayilaniyla devam edilir. */
    if (sysfs_write(id, "comp_algorithm", algorithm) != 0)
        LOGW("'%s' algoritmasi ayarlanamadi; cekirdek varsayilani kullanilacak.",
             algorithm);

    char szbuf[32];
    snprintf(szbuf, sizeof(szbuf), "%llu", (unsigned long long)disksize);
    if (sysfs_write(id, "disksize", szbuf) != 0) {
        LOGE("disksize ayarlanamadi; aygit kaldiriliyor.");
        char idbuf[16];
        snprintf(idbuf, sizeof(idbuf), "%d", id);
        (void)util_write_str(ZRAM_CONTROL_REMOVE, idbuf);
        return -1;
    }

    out->device_id = id;
    out->owned = 1;
    out->disksize = disksize;
    snprintf(out->dev_path, sizeof(out->dev_path), "/dev/zram%d", id);
    /* Istenen degil, gercekte ETKIN olan algoritmayi sakla ve raporla. */
    read_active_algo(id, out->algorithm, sizeof(out->algorithm));

    /* Takas basligi yaz (mkswap) ve etkinlestir (swapon). */
    char *const mkswap_argv[] = { "mkswap", out->dev_path, NULL };
    if (util_run(mkswap_argv) != 0) {
        LOGE("mkswap %s basarisiz.", out->dev_path);
        zram_teardown(out);
        return -1;
    }

    int flags = SWAP_FLAG_PREFER |
                ((priority << SWAP_FLAG_PRIO_SHIFT) & SWAP_FLAG_PRIO_MASK);
    if (swapon(out->dev_path, flags) != 0) {
        LOGE("swapon %s basarisiz: %s", out->dev_path, strerror(errno));
        zram_teardown(out);
        return -1;
    }

    LOGI("zram hazir: %s, boyut=%llu bayt, algoritma=%s, oncelik=%d",
         out->dev_path, (unsigned long long)out->disksize,
         out->algorithm, priority);
    return 0;
}

void zram_teardown(zram_dev_t *dev)
{
    if (!dev->owned || dev->device_id < 0)
        return;

    if (dev->dev_path[0]) {
        if (swapoff(dev->dev_path) != 0 && errno != EINVAL)
            LOGW("swapoff %s basarisiz: %s", dev->dev_path, strerror(errno));
    }

    char idbuf[16];
    snprintf(idbuf, sizeof(idbuf), "%d", dev->device_id);
    if (util_write_str(ZRAM_CONTROL_REMOVE, idbuf) != 0)
        LOGW("zram%d kaldirilamadi: %s", dev->device_id, strerror(errno));
    else
        LOGI("zram%d sokuldu.", dev->device_id);

    dev->owned = 0;
    dev->device_id = -1;
}
