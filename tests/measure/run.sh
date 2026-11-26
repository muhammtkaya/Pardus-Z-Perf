#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# run.sh — bir olcum senaryosunu kosar ve ham veriyi toplar.
#
# Senaryolar (A/B/C/D):
#   A  Baz          : zram YOK, zperfd YOK (cekirdek varsayilani)
#   B  Yalniz zram  : zram takas aygiti var, politika YOK
#   C  zperfd       : zram + politika, "Dengeli" profil
#   D  zperfd (agr.): zram + politika, "Akilli Tahta" profili
#
# Her senaryoda ayni yuk (tests/load/workload.sh) ve ayni on plan olceri
# (tests/prober/interaction_prober) kosar; toplanan ham veri analyze.py'ye girer.
#
# Kullanim (ROOT gerekir — zram/zperfd icin):
#   sudo tests/measure/run.sh A --duration 60 --mem-mb 1800
#   sudo tests/measure/run.sh C --duration 60 --mem-mb 1800
#
# Cikti: tests/measure/data/<senaryo>-<zaman>/  (CSV'ler + meta.txt)
set -u

SCEN="${1:-}"; shift || true
case "$SCEN" in A|B|C|D) ;; *)
  echo "Kullanim: $0 A|B|C|D [--duration SN] [--mem-mb MB] [--procs N]"; exit 1;; esac

DURATION=60; MEM_MB=1800; PROCS=6
while [ $# -gt 0 ]; do
  case "$1" in
    --duration) DURATION="$2"; shift 2;;
    --mem-mb)   MEM_MB="$2"; shift 2;;
    --procs)    PROCS="$2"; shift 2;;
    *) echo "bilinmeyen secenek: $1" >&2; exit 1;;
  esac
done

if [ "$(id -u)" -ne 0 ]; then
  echo "Bu betik root gerektirir (zram/zperfd)." >&2; exit 1
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ZPERFD="$ROOT/build/zperfd"
PROBER="$ROOT/build/interaction_prober"
SOCK="/run/zperf.sock"
TS="$(date +%Y%m%d-%H%M%S)"
OUT="$ROOT/tests/measure/data/${SCEN}-${TS}"
mkdir -p "$OUT"

# Gerekli ikililer derli degilse derle.
[ -x "$ZPERFD" ] || make -C "$ROOT" >/dev/null
[ -x "$PROBER" ] || make -C "$ROOT" prober >/dev/null

ZRAM_DEV=""
ZPERFD_PID=""
COLLECT_PID=""
WL_PID=""

note() { echo "$*" | tee -a "$OUT/meta.txt"; }

cleanup() {
  [ -n "$WL_PID" ] && kill "$WL_PID" 2>/dev/null
  [ -n "$COLLECT_PID" ] && kill "$COLLECT_PID" 2>/dev/null
  if [ -n "$ZPERFD_PID" ]; then kill -TERM "$ZPERFD_PID" 2>/dev/null; wait "$ZPERFD_PID" 2>/dev/null; fi
  # B senaryosunda elle kurdugumuz zram'i soyle kaldir.
  if [ -n "$ZRAM_DEV" ]; then
    swapoff "/dev/$ZRAM_DEV" 2>/dev/null
    echo "$ZRAM_DEV" | sed 's/zram//' > /sys/class/zram-control/hot_remove 2>/dev/null
  fi
  wait 2>/dev/null
}
trap cleanup EXIT INT TERM

# --- B icin: elle zram takas (politikasiz) ---
setup_zram_manual() {
  modprobe zram 2>/dev/null
  local id
  id="$(cat /sys/class/zram-control/hot_add 2>/dev/null)"
  [ -z "$id" ] && { note "zram hot_add basarisiz"; return 1; }
  ZRAM_DEV="zram${id}"
  local ram_kb total
  ram_kb="$(awk '/MemTotal/{print $2}' /proc/meminfo)"
  total="$(( ram_kb * 1024 ))"
  echo lz4 > "/sys/block/$ZRAM_DEV/comp_algorithm" 2>/dev/null || \
    echo zstd > "/sys/block/$ZRAM_DEV/comp_algorithm" 2>/dev/null
  echo "$total" > "/sys/block/$ZRAM_DEV/disksize"
  mkswap "/dev/$ZRAM_DEV" >/dev/null 2>&1
  swapon -p 100 "/dev/$ZRAM_DEV" 2>/dev/null
  note "B: zram kuruldu -> /dev/$ZRAM_DEV (algoritma=$(cat /sys/block/$ZRAM_DEV/comp_algorithm 2>/dev/null))"
}

# --- Arka plan toplayici: her 1 sn CSV satiri ---
collector() {
  echo "t_ms,some_avg10,full_avg10" > "$OUT/psi.csv"
  echo "t_ms,mem_avail_kb,swap_used_kb,pswpin,pswpout" > "$OUT/mem.csv"
  echo "t_ms,orig_bytes,compr_bytes,mem_used_bytes" > "$OUT/zram.csv"
  echo "t_ms,zperfd_cpu_jiffies,zperfd_rss_kb" > "$OUT/zperfd.csv"
  local zdev
  while :; do
    local t; t="$(date +%s%3N)"
    # PSI
    if [ -r /proc/pressure/memory ]; then
      local s f
      # satirlar: "some avg10=0.00 avg60=... ", $2 = "avg10=0.00"
      s="$(awk '/^some/{split($2,a,"=");print a[2]}' /proc/pressure/memory)"
      f="$(awk '/^full/{split($2,a,"=");print a[2]}' /proc/pressure/memory)"
      echo "$t,${s:-NA},${f:-NA}" >> "$OUT/psi.csv"
    fi
    # bellek + takas
    local ma su pin pout
    ma="$(awk '/MemAvailable/{print $2}' /proc/meminfo)"
    su="$(awk '/SwapTotal/{tot=$2}/SwapFree/{free=$2}END{print tot-free}' /proc/meminfo)"
    pin="$(awk '/^pswpin/{print $2}' /proc/vmstat)"
    pout="$(awk '/^pswpout/{print $2}' /proc/vmstat)"
    echo "$t,${ma:-NA},${su:-NA},${pin:-NA},${pout:-NA}" >> "$OUT/mem.csv"
    # zram mm_stat (ilk aygit)
    zdev="$(ls -d /sys/block/zram* 2>/dev/null | head -1)"
    if [ -n "$zdev" ] && [ -r "$zdev/mm_stat" ]; then
      read -r orig compr memused _ < "$zdev/mm_stat"
      echo "$t,${orig:-NA},${compr:-NA},${memused:-NA}" >> "$OUT/zram.csv"
    fi
    # zperfd CPU/RSS
    if [ -n "$ZPERFD_PID" ] && [ -r "/proc/$ZPERFD_PID/stat" ]; then
      local ut st rss
      ut="$(awk '{print $14}' /proc/$ZPERFD_PID/stat)"
      st="$(awk '{print $15}' /proc/$ZPERFD_PID/stat)"
      rss="$(awk '/VmRSS/{print $2}' /proc/$ZPERFD_PID/status)"
      echo "$t,$((ut+st)),${rss:-NA}" >> "$OUT/zperfd.csv"
    fi
    sleep 1
  done
}

note "=== Senaryo $SCEN | sure=${DURATION}s mem=${MEM_MB}MB procs=${PROCS} ==="
note "cekirdek: $(uname -r)"
note "RAM: $(awk '/MemTotal/{printf "%.1f GB",$2/1024/1024}' /proc/meminfo)"
dmesg --clear 2>/dev/null || true   # OOM sayimi icin temizle

case "$SCEN" in
  A) note "A: zram yok, zperfd yok" ;;
  B) setup_zram_manual ;;
  C|D)
     "$ZPERFD" --debug > "$OUT/zperfd.log" 2>&1 &
     ZPERFD_PID=$!
     note "zperfd baslatildi (pid $ZPERFD_PID)"
     sleep 2
     if [ "$SCEN" = "D" ]; then
       # Profili "Akilli Tahta"ya cek (saf stdlib istemci).
       PYTHONPATH="$ROOT/src/agent" python3 -c \
         "from zperf_agent.ipc import ZperfClient; c=ZperfClient('$SOCK'); c.connect(); c.set_profile('Akilli Tahta'); print('profil: Akilli Tahta')" \
         2>>"$OUT/meta.txt" || note "profil ayarlanamadi"
     fi
     ;;
esac

collector & COLLECT_PID=$!

# On plan olceri (M1) + yuk es zamanli.
note "yuk + prober basliyor"
"$ROOT/tests/load/workload.sh" --mem-mb "$MEM_MB" --procs "$PROCS" --duration "$DURATION" \
    > "$OUT/workload.log" 2>&1 &
WL_PID=$!

"$PROBER" --duration "$DURATION" --interval 50 --worksize 64 --out "$OUT/prober.csv" \
    2> "$OUT/prober_summary.txt"
cat "$OUT/prober_summary.txt" | tee -a "$OUT/meta.txt"

wait "$WL_PID" 2>/dev/null

# OOM sayimi
OOM="$(dmesg 2>/dev/null | grep -ic 'Out of memory\|oom-kill\|Killed process' || true)"
note "OOM/kill olaylari (dmesg): ${OOM:-0}"
dmesg 2>/dev/null | grep -i 'Out of memory\|oom-kill\|Killed process' > "$OUT/oom.log" || true

note "=== bitti -> $OUT ==="
echo "$OUT"
