#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# workload.sh — olcum icin kontrollu bellek baskisi uretir.
#
# Dusuk RAM'li bir cihazda (ornek 2 GB) ayni anda birden cok uygulamanin acik
# oldugu ders/ofis senaryosunu taklit eder: bir grup "uygulama" surekli bellek
# ayirip dokunur (working set), boylece sistem takasa/zram'e iner ve PSI yukselir.
#
# Iki kip:
#   - sentetik (varsayilan): stress-ng varsa onu, yoksa python ayiricilari kullanir.
#   - gercek (--apps): DISPLAY varsa gercek uygulamalari acar (firefox, libreoffice,
#     evince, bir video). Grafik oturum ve kurulu uygulama gerektirir.
#
# Kullanim:
#   tests/load/workload.sh --mem-mb 1800 --procs 6 --duration 60
#   tests/load/workload.sh --apps --duration 120
#
# Root GEREKMEZ. SIGINT/SIGTERM veya sure dolunca tum alt surecleri temizler.
set -u

MEM_MB=1500       # toplam hedef calisma kumesi (tum ayiricilar)
PROCS=6           # paralel "uygulama" sayisi
DURATION=60       # saniye
APPS=0            # 1 ise gercek uygulamalar

while [ $# -gt 0 ]; do
  case "$1" in
    --mem-mb)   MEM_MB="$2"; shift 2;;
    --procs)    PROCS="$2"; shift 2;;
    --duration) DURATION="$2"; shift 2;;
    --apps)     APPS=1; shift;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "bilinmeyen secenek: $1" >&2; exit 1;;
  esac
done

PIDS=()
cleanup() {
  echo "workload: temizlik..."
  for p in "${PIDS[@]:-}"; do kill "$p" 2>/dev/null; done
  # stress-ng ve actigimiz uygulamalar
  pkill -P $$ 2>/dev/null
  wait 2>/dev/null
}
trap cleanup EXIT INT TERM

echo "workload: mem=${MEM_MB}MB procs=${PROCS} sure=${DURATION}s apps=${APPS}"

if [ "$APPS" -eq 1 ]; then
  if [ -z "${DISPLAY:-}" ]; then
    echo "HATA: --apps icin grafik oturum (DISPLAY) gerekli." >&2; exit 1
  fi
  # Gercek uygulamalar — kurulu olanlari ac (yoklugunda sessizce gec).
  for cmd in \
      "firefox" \
      "libreoffice --writer" \
      "libreoffice --impress" \
      "evince" ; do
    bin="${cmd%% *}"
    if command -v "$bin" >/dev/null 2>&1; then
      $cmd >/dev/null 2>&1 &
      PIDS+=($!)
      echo "  acildi: $cmd"
      sleep 3
    fi
  done
  echo "workload: uygulamalar acik, ${DURATION}s bekleniyor."
  sleep "$DURATION"
  exit 0
fi

# --- Sentetik bellek baskisi ---
per=$(( MEM_MB / (PROCS>0?PROCS:1) ))

if command -v stress-ng >/dev/null 2>&1; then
  echo "workload: stress-ng ile ${PROCS}x${per}MB"
  stress-ng --vm "$PROCS" --vm-bytes "${per}M" --vm-keep \
            --vm-method write64 --timeout "${DURATION}s" &
  PIDS+=($!)
  wait "${PIDS[0]}"
  exit 0
fi

# stress-ng yoksa: python ayiricilar (her biri per MB tutar ve periyodik dokunur).
echo "workload: stress-ng yok -> python ayiricilar (${PROCS}x${per}MB)"
for i in $(seq 1 "$PROCS"); do
  python3 - "$per" "$DURATION" <<'PY' &
import sys, time
mb = int(sys.argv[1]); dur = int(sys.argv[2])
# mb MB'lik bir arabellek ayir ve surekli dokun (resident + dirty kalsin).
buf = bytearray(mb * 1024 * 1024)
end = time.time() + dur
step = 4096
while time.time() < end:
    for off in range(0, len(buf), step):
        buf[off] = (buf[off] + 1) & 0xFF
    time.sleep(0.05)
PY
  PIDS+=($!)
done

echo "workload: ${PROCS} ayirici calisiyor, ${DURATION}s."
sleep "$DURATION"
