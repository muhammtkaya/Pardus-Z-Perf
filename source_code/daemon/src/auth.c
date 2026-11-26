/* Sorumlu: Furkan (polkit sorgusu), Melih (polkit eylem dosyası).
 * Örnek kod: docs/06 bölüm 6.8. */
#include "auth.h"
#include "log.h"
#include "util.h"

#include <errno.h>

bool auth_owns_pid(uid_t client_uid, pid_t pid)
{
    uid_t uid;
    if (pid <= 1 || util_pid_uid(pid, &uid) < 0)
        return false;
    return uid == client_uid && uid >= ZP_MIN_UID;
}

int auth_polkit_check(sd_bus *bus, pid_t client_pid)
{
    (void) bus;
    /* TODO(G-52): org.freedesktop.PolicyKit1.Authority.CheckAuthorization
     *   subject = ("unix-process", {"pid": u, "start-time": t})
     *     start-time = /proc/<pid>/stat 22. alan (comm parantezinden sonra say)
     *   action_id = ZP_POLKIT_ACTION, details = {}, flags = 1, cancellation_id = ""
     *   yanıt (bba{ss}): ilk b = yetkili mi */
    LOG_WARN("auth_polkit_check(%d) henüz yazılmadı (TODO G-52)", (int) client_pid);
    return -ENOSYS;
}
