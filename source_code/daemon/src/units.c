/* Sorumlu: Furkan. Gereksinimler: G-04, G-06, G-07, G-08. Karar: TK-02.
 * Örnek kod: docs/06 bölüm 6.4 (StartTransientUnit) ve 6.5 (FreezeUnit/ThawUnit). */
#include "units.h"
#include "log.h"

#include <errno.h>

int units_adopt(sd_bus *bus, const char *unit, const pid_t *pids, size_t n, uid_t uid)
{
    (void) bus; (void) pids; (void) n; (void) uid;
    /* TODO(G-04): org.freedesktop.systemd1.Manager.StartTransientUnit
     *   ad = unit, mod = "fail"
     *   özellikler: Slice=user-<uid>.slice, MemoryAccounting=true,
     *               CollectMode=inactive-or-failed, Description, PIDs=au
     *   aux = boş a(sa(sv)) */
    LOG_WARN("units_adopt(%s) henüz yazılmadı (TODO G-04)", unit);
    return -ENOSYS;
}

int units_freeze(sd_bus *bus, const char *unit)
{
    (void) bus;
    /* TODO(G-06): Manager.FreezeUnit(s). Ardından cg_wait_frozen() ile bekle. */
    LOG_WARN("units_freeze(%s) henüz yazılmadı (TODO G-06)", unit);
    return -ENOSYS;
}

int units_thaw(sd_bus *bus, const char *unit)
{
    (void) bus;
    /* TODO(G-07): Manager.ThawUnit(s). */
    LOG_WARN("units_thaw(%s) henüz yazılmadı (TODO G-07)", unit);
    return -ENOSYS;
}

int units_thaw_all(sd_bus *bus)
{
    (void) bus;
    /* TODO(G-08): Manager.ListUnitsByPatterns(as states, as patterns) ile
     * ["zperf-*.scope"] birimlerini bul, her birine ThawUnit. Hata olsa da
     * diğerlerine devam et. Servis çökerse aynı işi scripts/zperf-thaw-all yapar. */
    LOG_WARN("units_thaw_all henüz yazılmadı (TODO G-08)");
    return -ENOSYS;
}
