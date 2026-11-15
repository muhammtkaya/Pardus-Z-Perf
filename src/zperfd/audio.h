/* SPDX-License-Identifier: GPL-3.0-or-later
 * audio.h: Bir uygulamanin su an ses calip calmadigini best-effort tespit.
 *
 * Amac: ses/video oynatan uygulamayi kademe 2'de kurban secmemek (dondurmak
 * ses kesintisine yol acar). PipeWire/PulseAudio'yu `pactl` ile sorgular.
 *
 * SINIRLAR (Pardus'ta dogrulanacak): pactl yoksa veya kullanici ses
 * oturumuna erisilemiyorsa (zperfd root'tur; kullanici PipeWire'i
 * XDG_RUNTIME_DIR ister) 0 doner -> davranis saf kurban secimine duser,
 * zararsizdir. Eslesme kok PID uzerindendir; alt surecte calan ses
 * kaciRILabilir. Ayrinti: docs/ipc-protocol.md degil, CHECKLIST'te not.
 */
#ifndef ZPERF_AUDIO_H
#define ZPERF_AUDIO_H

#include <sys/types.h>

/* pid (veya yakin kok) su an ses caliyorsa 1, aksi halde 0.
 * Sonuc kisa sure (birkac sn) onbelleklenir; sik cagrilabilir. */
int audio_is_playing(pid_t pid);

#endif /* ZPERF_AUDIO_H */
