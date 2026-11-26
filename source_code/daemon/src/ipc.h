/* Unix soketi üzerinden satır tabanlı JSON (TK-06). Protokol: docs/05 bölüm 6. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define ZP_SOCK_PATH   "/run/zperf.sock"
#define ZP_MAX_MSG     65536          /* aşan bağlantı kapatılır (T-33) */
#define ZP_MAX_CLIENTS 16
#define ZP_PROTO       1

enum zp_role { ROLE_NONE, ROLE_AGENT, ROLE_GUI };

struct zp_client {
    int          fd;                  /* -1 = boş yuva */
    pid_t        pid;                 /* SO_PEERCRED: istemci bunu taklit edemez */
    uid_t        uid;
    enum zp_role role;
    bool         subscribed;          /* olay akışı istiyor mu */
    size_t       len;
    char         buf[ZP_MAX_MSG];
};

struct zp_ctx;

int  ipc_listen(void);
void ipc_init_clients(struct zp_client *c, size_t n);
int  ipc_accept(struct zp_ctx *ctx);
/* Gelen veriyi okur, tamamlanan her satırı işler. <0 dönerse bağlantı kapatılmalı. */
int  ipc_read(struct zp_ctx *ctx, struct zp_client *c);
void ipc_close(struct zp_client *c);
int  ipc_send_line(struct zp_client *c, const char *json);
/* subscribe olmuş tüm istemcilere olay gönderir. */
void ipc_broadcast(struct zp_ctx *ctx, const char *json);
