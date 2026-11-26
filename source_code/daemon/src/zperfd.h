/* Servisin tüm durumu tek bir yapıda (docs/06 bölüm 4: global durum yok). */
#pragma once

#include <sys/types.h>
#include <systemd/sd-bus.h>

#include "apps.h"
#include "config.h"
#include "ipc.h"
#include "policy.h"

enum { PSI_K1, PSI_K2_SOME, PSI_K2_FULL, PSI_NFDS };

struct zp_ctx {
    struct zp_config         cfg;
    const char              *conf_path;
    const struct zp_profile *prof;
    sd_bus                  *bus;
    int                      sfd;              /* signalfd */
    int                      tfd;              /* timerfd, 1 sn */
    int                      lfd;              /* dinleme soketi */
    int                      psi_fd[PSI_NFDS];
    struct zp_client         clients[ZP_MAX_CLIENTS];
    struct zp_apps           apps;
    struct policy_state      pol;
    pid_t                    focused_pid;      /* ajanın son bildirdiği */
};

/* Profili değiştirir ve PSI tetikleyicilerini yeni eşiklerle yeniden kurar (G-12). */
int zp_apply_profile(struct zp_ctx *ctx, const char *name);
