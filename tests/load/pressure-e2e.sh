#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Uctan uca POLITIKA testi: gercek zperfd + iki yonetilen "uygulama" +
# bellek sinirli bir scope icinde stress-ng.
#
# Neden sinirli scope? Gelistirme makinesinde RAM bol (ornegin 16 GB) oldugu
# icin stress-ng sistemi baskiya sokamaz (PSI %0 kalir). stress-ng'yi
# MemoryHigh ile sinirlanan gecici bir systemd scope'ta calistirmak,
# masaustunu kilitlemeden GERCEK bellek-durma (memstall) uretir; bu durma
# sistem geneli /proc/pressure/memory'ye yansir ve zperfd'yi uyandirir.
#
# Senaryo:
#   appA ve appB (her biri ~APP_MB MB ayiran surecler) yonetime alinir;
#   odak once A'ya sonra B'ye verilir -> A en eski = beklenen KURBAN, B odakta.
#   Baski boyunca zaman serisi toplanir; sonunda dogrulanir:
#     [1] PSI baski olayi alindi        (zperfd tetikleyicisi uyandi)
#     [2] odaktaki B HIC dondurulmadi
#     [3] kademe 2'ye cikildiysa A donduruldu (+ "frozen" olayi yayinlandi)
#     [4] kapanista donmus uygulama / zram / soket kalmadi
#   Ayrica polkit yolu (normal kullanici ayar komutu) ve baskasinin PID'ine
#   odak reddi denenir.
#
# Kullanim (depo kokunden):  sudo tests/load/pressure-e2e.sh [cikti_dizini]
# Ayarlar (ortam):  STRESS_MEM=2G  HIGH=400M  DUR=40  APP_MB=300
set -u

if [ "$(id -u)" -ne 0 ]; then echo "root gerekli: sudo $0"; exit 77; fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/build/zperfd"
OUT="${1:-$ROOT/pressure-sonuc}"
STRESS_MEM="${STRESS_MEM:-2G}"
HIGH="${HIGH:-400M}"
DUR="${DUR:-40}"
APP_MB="${APP_MB:-300}"
SOCK=/run/zperf.sock
CG=/sys/fs/cgroup/zperf.slice
POLICY_SRC="$ROOT/polkit/org.pardus.zperf.policy"
POLICY_DST=/usr/share/polkit-1/actions/org.pardus.zperf.policy
USER_NAME="${SUDO_USER:-}"

for t in stress-ng systemd-run python3; do
  command -v "$t" >/dev/null || { echo "eksik arac: $t"; exit 1; }
done
[ -x "$BIN" ] || { echo "once derleyin: make"; exit 1; }
if pgrep -x zperfd >/dev/null; then echo "zperfd zaten calisiyor; once durdurun."; exit 1; fi

mkdir -p "$OUT"
LOG="$OUT/zperfd.log"
fails=0
ok()   { echo "  [GECTI] $*"; }
bad()  { echo "  [KALDI] $*"; fails=$((fails+1)); }
note() { echo "  [NOT]   $*"; }

# Python istemcisi (depodaki ajan kutuphanesi) ile tek komut
zp() { PYTHONPATH="$ROOT/src/agent" python3 -c "
import sys, json
from zperf_agent.ipc import ZperfClient
c = ZperfClient('$SOCK')
$1
c.close()"; }

PIDS=()
POLICY_INSTALLED=0
cleanup() {
  for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null; done
  if [ -n "${ZP:-}" ] && kill -0 "$ZP" 2>/dev/null; then kill -INT "$ZP"; wait "$ZP"; fi
  [ "$POLICY_INSTALLED" = 1 ] && rm -f "$POLICY_DST"
  [ -n "$USER_NAME" ] && chown -R "$USER_NAME:" "$OUT" 2>/dev/null
}
trap cleanup EXIT

echo "##### zperfd baslatiliyor #####"
"$BIN" --debug > "$LOG" 2>&1 &
ZP=$!
for _ in $(seq 50); do [ -S "$SOCK" ] && break; sleep 0.1; done
[ -S "$SOCK" ] || { echo "soket olusmadi"; cat "$LOG"; exit 1; }
grep -m1 "zram hazir\|zram takasi zaten" "$LOG"
stat -c 'soket izni: %A' "$SOCK"

# --- olay dinleyici: servisin ittigi dondu/cozuldu olaylarini kaydet ---
PYTHONPATH="$ROOT/src/agent" python3 -u -c "
import socket, sys
s = socket.socket(socket.AF_UNIX); s.connect('$SOCK')
while True:
    d = s.recv(4096)
    if not d: break
    sys.stdout.write(d.decode())
" > "$OUT/events.log" 2>&1 &
PIDS+=($!)

echo; echo "##### Yetki testleri #####"
if [ -n "$USER_NAME" ]; then
  # Normal kullanici: kendi olmayan PID'e (init) odak -> reddedilmeli
  out=$(sudo -u "$USER_NAME" env PYTHONPATH="$ROOT/src/agent" python3 -c "
from zperf_agent.ipc import ZperfClient
c = ZperfClient('$SOCK'); c.send_focus('init', 1); c.get_status()
print(c.last_error)" 2>&1)
  echo "$out" | grep -q "pid-yetkisi-yok" && ok "normal kullanici baskasinin PID'ine odak veremiyor" \
                                          || bad "baskasinin PID'ine odak reddedilmedi: $out"

  # Normal kullanici: polkit ile profil degistirme (etkin oturumda izinli olmali)
  if [ ! -e "$POLICY_DST" ]; then cp "$POLICY_SRC" "$POLICY_DST"; POLICY_INSTALLED=1; sleep 1; fi
  out=$(sudo -u "$USER_NAME" env PYTHONPATH="$ROOT/src/agent" python3 -c "
from zperf_agent.ipc import ZperfClient
c = ZperfClient('$SOCK'); c.set_profile('Ofis'); st = c.get_status()
print(st.get('profile'), c.last_error)" 2>&1)
  echo "  kullanici profil denemesi -> $out"
  if echo "$out" | grep -q "^Ofis None"; then ok "polkit: etkin oturumdaki kullanici profili degistirebildi"
  else note "polkit izni vermedi (oturum 'active' degilse beklenen; GUI'yi masaustunden deneyin)"; fi
  zp "c.set_profile('Dengeli')"
else
  note "SUDO_USER yok; normal kullanici yetki testleri atlandi"
fi

echo; echo "##### Yonetilen uygulamalar (A=kurban adayi, B=odakta) #####"
spawn_app() {  # bellegi 3 sn SONRA ayirir: o arada cgroup'a tasinmis olur
  python3 -c "
import time, sys
time.sleep(3)
b = b'\x01' * ($APP_MB << 20)
time.sleep(3600)" >/dev/null 2>&1 &
  echo $!
}
A=$(spawn_app); PIDS+=("$A")
zp "c.send_focus('appa', $A)"
sleep 0.5
B=$(spawn_app); PIDS+=("$B")
zp "c.send_focus('appb', $B)"
sleep 5
for app in appa appb; do
  m=$(cat "$CG/app-$app/memory.current" 2>/dev/null || echo -1)
  echo "  app-$app memory.current = $((m / 1048576)) MB"
done
memA0=$(cat "$CG/app-appa/memory.current" 2>/dev/null || echo 0)
[ "$memA0" -gt $((APP_MB * 1048576 / 2)) ] && ok "A'nin bellegi kendi cgroup'una yuklendi" \
                                           || bad "A'nin bellegi cgroup'ta gorunmuyor ($memA0)"

echo; echo "##### Baski: stress-ng ${STRESS_MEM} (MemoryHigh=${HIGH}) ${DUR} sn #####"
systemd-run --scope --quiet --unit="zperf-stress-$$" \
  -p MemoryHigh="$HIGH" -p MemorySwapMax=infinity \
  stress-ng --vm 2 --vm-bytes "$STRESS_MEM" --vm-keep --timeout "${DUR}s" \
  > "$OUT/stress.log" 2>&1 &
SP=$!

echo "t,some_avg10,full_avg10,swap_used_mb,appa_mb,appb_mb,appa_frozen,appb_frozen" > "$OUT/timeline.csv"
everA=0; everB=0; maxsome=0; t=0
while kill -0 "$SP" 2>/dev/null; do
  some=$(awk '/^some/{split($2,a,"=");print a[2]}' /proc/pressure/memory)
  full=$(awk '/^full/{split($2,a,"=");print a[2]}' /proc/pressure/memory)
  swap=$(awk '/SwapTotal/{t=$2}/SwapFree/{f=$2}END{print int((t-f)/1024)}' /proc/meminfo)
  ma=$(( $(cat "$CG/app-appa/memory.current" 2>/dev/null || echo 0) / 1048576 ))
  mb=$(( $(cat "$CG/app-appb/memory.current" 2>/dev/null || echo 0) / 1048576 ))
  fa=$(cat "$CG/app-appa/cgroup.freeze" 2>/dev/null || echo 0)
  fb=$(cat "$CG/app-appb/cgroup.freeze" 2>/dev/null || echo 0)
  [ "$fa" = 1 ] && everA=1
  [ "$fb" = 1 ] && everB=1
  maxsome=$(awk -v a="$maxsome" -v b="$some" 'BEGIN{print (b>a)?b:a}')
  echo "$t,$some,$full,$swap,$ma,$mb,$fa,$fb" >> "$OUT/timeline.csv"
  printf "  t=%3ss some=%6s full=%6s swap=%5sMB A=%4sMB%s B=%4sMB%s\n" \
    "$t" "$some" "$full" "$swap" "$ma" "$([ "$fa" = 1 ] && echo '(DONDU)' || echo '       ')" \
    "$mb" "$([ "$fb" = 1 ] && echo '(DONDU)')"
  sleep 1; t=$((t+1))
done
wait "$SP"
sleep 2

echo; echo "##### Dogrulama #####"
nev=$(grep -c "BASKI OLAYI" "$LOG")
t2=$(grep -c "KADEME 2" "$LOG")
memA1=$(cat "$CG/app-appa/memory.current" 2>/dev/null || echo 0)
echo "  en yuksek some avg10 = %$maxsome | BASKI OLAYI = $nev | KADEME 2 okumasi = $t2"
echo "  A bellegi: $((memA0/1048576)) MB -> $((memA1/1048576)) MB (reclaim)"
[ "$nev" -gt 0 ] && ok "PSI tetikleyicisi zperfd'yi uyandirdi" \
                 || bad "hic BASKI OLAYI yok (baski yetersiz? HIGH'i dusurun / STRESS_MEM'i artirin)"
[ "$everB" = 0 ] && ok "odaktaki B hic dondurulmadi" || bad "odaktaki B donduruldu!"
if [ "$t2" -gt 0 ]; then
  [ "$everA" = 1 ] && ok "kademe 2'de en eski uygulama (A) donduruldu" || bad "kademe 2'ye cikildi ama A donmadi"
  grep -q '"event":"frozen","name":"appa"' "$OUT/events.log" \
    && ok "'frozen' olayi istemcilere yayinlandi" || bad "'frozen' olayi gelmedi"
else
  note "kademe 2 esigine ulasilmadi; dondurma denenmedi (HIGH'i dusurerek tekrar deneyin)"
fi
[ "$memA1" -lt "$memA0" ] && ok "A'nin bellegi reclaim ile azaldi" || note "A'nin bellegi azalmadi"

echo; echo "##### Kapanis #####"
for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null; done
kill -INT "$ZP"; wait "$ZP"; rc=$?; ZP=
[ $rc -eq 0 ] && ok "zperfd temiz kapandi (rc=0)" || bad "zperfd cikis kodu $rc"
grep -q zram /proc/swaps && bad "zram takasi kaldi" || ok "zram takasi sokuldu"
[ -e "$SOCK" ] && bad "soket kaldi" || ok "soket silindi"
leftover=$(find "$CG" -name cgroup.freeze -exec grep -l 1 {} + 2>/dev/null | wc -l)
[ "$leftover" -eq 0 ] && ok "donmus cgroup kalmadi" || bad "$leftover donmus cgroup kaldi"

echo; echo "Ciktilar: $OUT (zperfd.log, events.log, timeline.csv, stress.log)"
echo "SONUC: $([ $fails -eq 0 ] && echo GECTI || echo "$fails KONTROL KALDI")"
[ $fails -eq 0 ]
