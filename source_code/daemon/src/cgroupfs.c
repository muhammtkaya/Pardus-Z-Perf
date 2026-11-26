#include "cgroupfs.h"
#include "util.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int cg_path_of_pid(pid_t pid, char *buf, size_t n)
{
    char path[64], data[1024];
    snprintf(path, sizeof path, "/proc/%d/cgroup", (int) pid);
    int r = util_read_file(path, data, sizeof data);
    if (r < 0)
        return r;
    /* cgroup v2'de tek satır vardır: "0::/user.slice/..." */
    const char *p = strstr(data, "0::");
    if (!p)
        return -ENOENT;
    p += 3;
    size_t len = strcspn(p, "\n");
    if (len + 1 > n)
        return -ENAMETOOLONG;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return 0;
}

int cg_read_u64(const char *cg, const char *file, uint64_t *out)
{
    char path[PATH_MAX], data[64];
    snprintf(path, sizeof path, ZP_CG_ROOT "%s/%s", cg, file);
    int r = util_read_file(path, data, sizeof data);
    if (r < 0)
        return r;
    char *end;
    errno = 0;
    unsigned long long v = strtoull(data, &end, 10);
    if (errno || end == data)
        return -EINVAL;
    *out = (uint64_t) v;
    return 0;
}

int cg_reclaim(const char *cg, uint64_t bytes)
{
    char path[PATH_MAX], val[32];
    snprintf(path, sizeof path, ZP_CG_ROOT "%s/memory.reclaim", cg);
    snprintf(val, sizeof val, "%" PRIu64, bytes);
    int r = util_write_file(path, val);
    /* EAGAIN: çekirdek istenen miktarın tamamını geri kazanamadı (sayfalar sıcak,
     * zram dolu...). Yapabildiği kadarını yapmıştır; hata değildir. */
    return r == -EAGAIN ? 0 : r;
}

int cg_wait_frozen(const char *cg, int timeout_ms)
{
    char path[PATH_MAX], data[256];
    snprintf(path, sizeof path, ZP_CG_ROOT "%s/cgroup.events", cg);
    const struct timespec step = { 0, 10 * 1000 * 1000 };   /* 10 ms */
    for (int waited = 0; waited <= timeout_ms; waited += 10) {
        int r = util_read_file(path, data, sizeof data);
        if (r < 0)
            return r;
        if (strstr(data, "frozen 1"))
            return 0;
        nanosleep(&step, NULL);
    }
    return -ETIMEDOUT;
}
