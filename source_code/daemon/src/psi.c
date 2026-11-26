#include "psi.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int psi_trigger_open(const char *spec)
{
    int fd = open(ZP_PSI_MEMORY, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    /* Çekirdek belgesindeki örnek NUL karakterini de yazar. Pencere 500 ms ile
     * 10 s arasında olmalı; aksi halde write() EINVAL döner. Her tetikleyici
     * kendi fd'sini ister. */
    if (write(fd, spec, strlen(spec) + 1) < 0) {
        int e = errno;
        close(fd);
        return -e;
    }
    return fd;
}

int psi_read_avgs(struct psi_avgs *out)
{
    char buf[256];
    int r = util_read_file(ZP_PSI_MEMORY, buf, sizeof buf);
    if (r < 0)
        return r;
    /* some avg10=0.00 avg60=0.00 avg300=0.00 total=0
     * full avg10=0.00 avg60=0.00 avg300=0.00 total=0 */
    const char *full = strstr(buf, "full ");
    if (sscanf(buf, "some avg10=%lf avg60=%lf", &out->some10, &out->some60) != 2 ||
        !full || sscanf(full, "full avg10=%lf avg60=%lf", &out->full10, &out->full60) != 2)
        return -EINVAL;
    return 0;
}
