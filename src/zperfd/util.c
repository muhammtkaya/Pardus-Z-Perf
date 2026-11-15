/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "util.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int util_write_str(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    size_t len = strlen(value);
    ssize_t n = write(fd, value, len);
    int saved = errno;
    close(fd);

    if (n < 0 || (size_t)n != len) {
        errno = saved;
        return -1;
    }
    return 0;
}

long util_read_str(const char *path, char *buf, size_t len)
{
    if (len == 0)
        return -1;

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    ssize_t n = read(fd, buf, len - 1);
    int saved = errno;
    close(fd);

    if (n < 0) {
        errno = saved;
        return -1;
    }
    buf[n] = '\0';
    return (long)n;
}

int util_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

int util_run(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        LOGE("fork basarisiz: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        /* Cocuk: kabuk kullanmadan dogrudan calistir. */
        execvp(argv[0], argv);
        /* Buraya gelindiyse exec basarisiz oldu. */
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

uint64_t util_total_ram_bytes(void)
{
    char buf[4096];
    if (util_read_str("/proc/meminfo", buf, sizeof(buf)) < 0)
        return 0;

    char *p = strstr(buf, "MemTotal:");
    if (!p)
        return 0;

    unsigned long long kb = 0;
    if (sscanf(p + strlen("MemTotal:"), " %llu", &kb) != 1)
        return 0;

    return (uint64_t)kb * 1024ULL;
}

int util_zram_swap_active(void)
{
    char buf[8192];
    if (util_read_str("/proc/swaps", buf, sizeof(buf)) < 0)
        return 0;
    return strstr(buf, "zram") != NULL;
}

long util_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
