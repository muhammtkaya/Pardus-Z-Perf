/* cgroup v2 dosya sistemi: süreç → cgroup yolu, istatistik okuma, geri kazanım.
 * Yollar "/user.slice/..." biçimindedir; başlarına ZP_CG_ROOT eklenir. */
#pragma once

#include <stdint.h>
#include <sys/types.h>

#define ZP_CG_ROOT "/sys/fs/cgroup"

/* /proc/<pid>/cgroup içindeki "0::<yol>" satırından yolu alır. */
int cg_path_of_pid(pid_t pid, char *buf, size_t n);

/* <cg>/<file> içindeki tek sayıyı okur (memory.current, memory.swap.current...). */
int cg_read_u64(const char *cg, const char *file, uint64_t *out);

/* memory.reclaim'e yazar. EAGAIN (kısmi başarı) 0 döner. */
int cg_reclaim(const char *cg, uint64_t bytes);

/* cgroup.events içinde "frozen 1" görülene kadar en fazla timeout_ms bekler. */
int cg_wait_frozen(const char *cg, int timeout_ms);
