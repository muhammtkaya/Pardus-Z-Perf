/* SPDX-License-Identifier: GPL-3.0-or-later
 * ipc.h: Unix soketi (/run/zperf.sock) uzerinden satir tabanli JSON sunucusu.
 *
 * Ajan ve arayuz servise bu soketle baglanir. Mesajlar tek satir JSON'dur:
 *   {"type":"focus","name":"firefox","pid":1234}
 *   {"type":"status"}            -> servis tek satir JSON durum dondurur
 *
 * Sunucu, epoll ile ana donguye katilir: dinleme ve istemci soketleri a
 * ipc_init'e verilen epoll ornegine eklenir; ana dongu ipc'ye ait bir fd
 * icin ipc_handle_fd cagirir.
 */
#ifndef ZPERF_IPC_H
#define ZPERF_IPC_H

#include <stddef.h>
#include <sys/types.h>

/* Odak mesaji gelince cagrilir (ornek: policy_on_focus). */
typedef void (*ipc_focus_fn)(const char *name, pid_t pid);
/* Durum istenince, tek satirlik JSON'u buf'a yazar. */
typedef void (*ipc_status_fn)(char *buf, size_t len);
/* Ayar komutu (ornek: never_freeze) gelince cagrilir. */
typedef void (*ipc_cmd_fn)(const char *cmd, const char *name, int value);

/* Soketi kurar ve epfd'ye ekler. 0 basarili, -1 hata. */
int  ipc_init(int epfd, const char *path,
              ipc_focus_fn on_focus, ipc_status_fn status);

/* Arayuzden gelen ayar komutlari icin isleyici (istege bagli). */
void ipc_set_cmd_handler(ipc_cmd_fn fn);

/* fd, ipc'ye ait (dinleme veya istemci) mi? */
int  ipc_owns_fd(int fd);

/* ipc'ye ait bir fd'de epoll olayi oldugunda cagrilir. */
void ipc_handle_fd(int fd);

/* Soketleri kapatir ve dosyayi siler. */
void ipc_shutdown(void);

#endif /* ZPERF_IPC_H */
