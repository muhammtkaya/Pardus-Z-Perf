/* İstemci yetkilendirmesi (G-52, docs/05 bölüm 7). */
#pragma once

#include <stdbool.h>
#include <sys/types.h>
#include <systemd/sd-bus.h>

#define ZP_POLKIT_ACTION "io.github.zperf.manage"
#define ZP_MIN_UID       1000

/* pid, client_uid'ye ait ve UID'i ZP_MIN_UID veya üstü mü? Böylece bir kullanıcı
 * başkasının ya da root'un sürecini bildiremez. */
bool auth_owns_pid(uid_t client_uid, pid_t pid);

/* polkit'e "client_pid bu eylem için yetkili mi?" diye sorar.
 * 1 = yetkili, 0 = değil, <0 = hata. */
int auth_polkit_check(sd_bus *bus, pid_t client_pid);
