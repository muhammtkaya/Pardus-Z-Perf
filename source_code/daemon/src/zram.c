/* Sorumlu: Furkan. Gereksinim: G-02. Örnek kod ve doğru sıra: docs/06 bölüm 6.7. */
#include "zram.h"
#include "log.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int zram_active(char *device, size_t n)
{
    char buf[4096];
    int r = util_read_file("/proc/swaps", buf, sizeof buf);
    if (r < 0)
        return r;
    char *p = strstr(buf, "/dev/zram");
    if (!p)
        return 0;
    if (device && n)
        snprintf(device, n, "%.*s", (int) strcspn(p, " \t"), p);
    return 1;
}

int zram_setup(const struct zp_profile *p)
{
    char dev[32];
    if (zram_active(dev, sizeof dev) == 1) {
        LOG_INFO("zram zaten etkin (%s), yeni aygıt kurulmuyor", dev);
        /* TODO(G-02): Yine de swappiness/page-cluster profile göre ayarlanmalı mı?
         * Karar verip burada uygulayın. */
        return 0;
    }
    (void) p;
    /* TODO(G-02): Sırasıyla:
     *  1. Eski vm.swappiness değerini ZP_STATE_DIR/swappiness.orig dosyasına yaz
     *     (dosya zaten varsa üzerine yazma; ilk kurulumdaki değer korunmalı).
     *  2. util_run(modprobe zram). zram0 meşgulse /sys/class/zram-control/hot_add.
     *  3. comp_algorithm ← p->algorithm  (disksize'dan ÖNCE)
     *  4. disksize ← MemTotal'in %50'si (/proc/meminfo)
     *  5. util_run(/usr/sbin/mkswap /dev/zramN)
     *  6. swapon(2), SWAP_FLAG_PREFER | öncelik 100
     *  7. /proc/sys/vm/swappiness ← p->swappiness, /proc/sys/vm/page-cluster ← 0
     * Başarısızlıkta servis durmaz; uyarı yazar ve disk takasıyla devam eder (T-31). */
    LOG_WARN("zram_setup henüz yazılmadı (TODO G-02)");
    return -ENOSYS;
}

int zram_stats(struct zram_stats *out)
{
    memset(out, 0, sizeof *out);
    /* TODO(G-14): zram_active() ile aygıtı bul; /sys/block/zramN/{disksize,
     * comp_algorithm,mm_stat} oku. comp_algorithm çıktısında seçili olan köşeli
     * parantez içindedir: "[lz4] lz4hc zstd deflate". */
    return -ENOSYS;
}
