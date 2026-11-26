#!/bin/sh
# Yük betiği: her çalıştırmada aynı yükü aynı sırayla üretir (G-42, docs/07 bölüm 5.2).
# Sorumlu: Melih.
#
# Kullanım: load.sh -c <A|B|C|D> -r <tekrar no> [-t en fazla sekme] [-m sekme başına MB]
# Çıktı:    results/raw/<yapılandırma>_<tekrar>/ altında probe.csv, collect.csv,
#           load.log (her sekmenin açılış zamanı), events.log (servis EVT satırları)
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
CONFIG=""; RUN=1; MAX_TABS=25; TAB_MB=120; VIDEO="${VIDEO:-$HOME/zperf-video.mp4}"

while getopts c:r:t:m: o; do
    case $o in
        c) CONFIG=$OPTARG ;; r) RUN=$OPTARG ;; t) MAX_TABS=$OPTARG ;; m) TAB_MB=$OPTARG ;;
        *) echo "Kullanım: $0 -c A|B|C|D -r N [-t sekme] [-m MB]" >&2; exit 2 ;;
    esac
done
[ -n "$CONFIG" ] || { echo "-c gerekli" >&2; exit 2; }

OUT="$HERE/../../results/raw/${CONFIG}_${RUN}"
mkdir -p "$OUT"
echo "yapılandırma=$CONFIG tekrar=$RUN sekme_mb=$TAB_MB en_fazla=$MAX_TABS" > "$OUT/load.log"

# TODO(G-42): Adımlar (zamanlar docs/07 bölüm 5.2 ile aynı olmalı):
#  t=0    : collect.py ve probe'u arka planda başlat, PID'lerini sakla
#  t=10   : LibreOffice Writer ile bir belge, Mousepad ile ~/buyuk.txt aç
#  t=30   : firefox-esr ile $VIDEO (döngüde) aç; bu pencere odakta kalmalı
#  t=40'tan itibaren her 20 sn:
#           firefox-esr --new-tab "file://$HERE/tab.html?mb=$TAB_MB"
#           açılış zamanını load.log'a yaz
#  Durma koşulu: MAX_TABS sekmeye ulaşıldı, ya da probe.csv'de tek seferde
#           30 sn'yi aşan takılma, ya da toplam 15 dk
#  Sonunda: collect/probe'u durdur, `journalctl -u zperfd -o cat --since @<başlangıç>`
#           içinden '^EVT' satırlarını events.log'a kopyala, OOM satırları için
#           `journalctl -k --since @<başlangıç> | grep -c 'Out of memory'` (M4)
echo "load.sh henüz yazılmadı (TODO G-42)" >&2
exit 1
