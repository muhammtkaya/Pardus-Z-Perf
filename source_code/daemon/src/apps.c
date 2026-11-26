#include "apps.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

struct zp_app *apps_find_pid(struct zp_apps *t, pid_t pid)
{
    for (size_t i = 0; i < t->n; i++)
        if (t->a[i].pid == pid)
            return &t->a[i];
    /* TODO(G-07): Odak olayındaki PID, kök sürecin bir alt süreci olabilir
     * (örn. bazı uygulamalarda pencereyi alt süreç açar). Bulunamazsa
     * cg_path_of_pid() ile PID'in scope'u bulunup tablodaki cgroup alanıyla
     * karşılaştırılabilir. */
    return NULL;
}

struct zp_app *apps_add(struct zp_apps *t, pid_t pid, uid_t uid, const char *comm)
{
    if (t->n == ZP_MAX_APPS)
        return NULL;
    struct zp_app *a = &t->a[t->n++];
    memset(a, 0, sizeof *a);
    a->pid = pid;
    a->uid = uid;
    snprintf(a->comm, sizeof a->comm, "%s", comm);
    a->excluded = apps_is_system_component(comm);
    return a;
}

void apps_remove(struct zp_apps *t, struct zp_app *app)
{
    size_t i = (size_t) (app - t->a);
    if (i >= t->n)
        return;
    t->a[i] = t->a[--t->n];   /* sıra önemli değil; sonuncuyu boşluğa taşı */
}

int apps_unit_name(const char *comm, pid_t pid, char *out, size_t n)
{
    char clean[33];
    size_t j = 0;
    for (const char *p = comm; *p && j < sizeof clean - 1; p++) {
        unsigned char c = (unsigned char) tolower((unsigned char) *p);
        clean[j++] = (isalnum(c) || c == '_' || c == '-') ? (char) c : '-';
    }
    clean[j] = '\0';
    if (j == 0)
        snprintf(clean, sizeof clean, "app");
    int r = snprintf(out, n, "zperf-%s-%d.scope", clean, (int) pid);
    return (r < 0 || (size_t) r >= n) ? -ENAMETOOLONG : 0;
}

bool apps_is_system_component(const char *comm)
{
    /* /proc/<pid>/comm en fazla 15 karakterdir; uzun adlar kırpılmış olarak gelir. */
    static const char *const sys[] = {
        "Xorg", "xfwm4", "xfce4-panel", "xfdesktop", "xfce4-session", "xfsettingsd",
        "lightdm", "pipewire", "wireplumber", "pipewire-pulse", "pulseaudio",
        "polkit-gnome-au", "xfce-polkit", "zperf-agent", "zperf-gui",
    };
    for (size_t i = 0; i < sizeof sys / sizeof sys[0]; i++)
        if (strcmp(comm, sys[i]) == 0)
            return true;
    return false;
}
