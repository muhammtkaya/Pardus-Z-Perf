#include "util.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int g_debug = 0;

int util_read_file(const char *path, char *buf, size_t n)
{
    if (n == 0)
        return -EINVAL;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    size_t len = 0;
    while (len < n - 1) {
        ssize_t r = read(fd, buf + len, n - 1 - len);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            int e = errno;
            close(fd);
            return -e;
        }
        if (r == 0)
            break;
        len += (size_t) r;
    }
    buf[len] = '\0';
    close(fd);
    return (int) len;
}

int util_write_file(const char *path, const char *s)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    size_t len = strlen(s);
    ssize_t r = write(fd, s, len);
    /* sysfs/cgroupfs dosyaları değeri tek bir yazımda bekler; kısmi yazım
     * çekirdeğin değeri reddettiği anlamına gelir. */
    int ret = r < 0 ? -errno : ((size_t) r == len ? 0 : -EIO);
    close(fd);
    if (ret < 0 && ret != -EAGAIN)
        LOG_DBG("yazma %s <- '%s': %s", path, s, strerror(-ret));
    return ret;
}

int util_run(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -errno;
    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    }
    int st;
    while (waitpid(pid, &st, 0) < 0) {
        if (errno != EINTR)
            return -errno;
    }
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -EIO;
}

int util_pid_comm(pid_t pid, char *buf, size_t n)
{
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/comm", (int) pid);
    int r = util_read_file(path, buf, n);
    if (r < 0)
        return r;
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

int util_pid_uid(pid_t pid, uid_t *uid)
{
    char path[64], buf[4096];
    snprintf(path, sizeof path, "/proc/%d/status", (int) pid);
    int r = util_read_file(path, buf, sizeof buf);
    if (r < 0)
        return r;
    /* "Uid:\t<gerçek>\t<etkin>\t<kayıtlı>\t<fs>"; gerçek UID kullanılır. */
    const char *p = strstr(buf, "\nUid:");
    unsigned u;
    if (!p || sscanf(p + 5, "%u", &u) != 1)
        return -EINVAL;
    *uid = (uid_t) u;
    return 0;
}

static double clock_s(clockid_t id)
{
    struct timespec ts;
    clock_gettime(id, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

double util_now(void)       { return clock_s(CLOCK_MONOTONIC); }
double util_wallclock(void) { return clock_s(CLOCK_REALTIME); }
