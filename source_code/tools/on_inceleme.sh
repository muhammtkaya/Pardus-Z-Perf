#!/usr/bin/env bash
# Z-Performans ön inceleme: Pardus üzerinde öneri öncesi kontroller.
# Kullanım (sudo OLMADAN çalıştırın; oturum bilgileri sudo altında kaybolur):
#   bash on_inceleme.sh          -> yalnızca okur, sistemde hiçbir şeyi değiştirmez
#   bash on_inceleme.sh --zram   -> ayrıca zram modülünü geçici yükleyip özelliklerini okur
#                                   (sudo şifresi sorar; yalnızca sanal makinede kullanın)
# Tüm çıktı on_inceleme_<makine>_<tarih>.txt dosyasına da kaydedilir.

# swapon, zramctl, modinfo Debian'da /usr/sbin altında; normal kullanıcının PATH'inde yok.
export PATH="$PATH:/usr/sbin:/sbin"
OUT="on_inceleme_$(hostname)_$(date +%Y%m%d_%H%M).txt"
exec > >(tee "$OUT") 2>&1

b() { printf '\n===== %s =====\n' "$1"; }
r() { printf '$ %s\n' "$1"; eval "$1" 2>&1 || true; }

b "1. Sürüm ve çekirdek"
r 'grep -E "^(PRETTY_NAME|VERSION_ID)=" /etc/os-release'
r 'uname -r'
r 'cat /proc/cmdline'

b "2. Masaüstü oturumu"
r 'echo "XDG_SESSION_TYPE=$XDG_SESSION_TYPE  XDG_CURRENT_DESKTOP=$XDG_CURRENT_DESKTOP"'

b "3. Takas ve zram'in varsayılan durumu"
r 'swapon --show'
r 'zramctl'
r 'systemctl list-unit-files --no-pager | grep -iE "zram|swap"'
r 'dpkg -l | grep -iE "zram|earlyoom|nohang" | cut -c1-90'
r 'echo "swappiness=$(cat /proc/sys/vm/swappiness)  page-cluster=$(cat /proc/sys/vm/page-cluster)"'
r 'free -h'

b "4. PSI"
r 'ls /proc/pressure'
r 'cat /proc/pressure/memory'
r 'grep -E "CONFIG_PSI" /boot/config-$(uname -r)'

b "5. cgroup v2"
r 'stat -fc %T /sys/fs/cgroup'
r 'cat /sys/fs/cgroup/cgroup.controllers'
r 'cat /sys/fs/cgroup/cgroup.subtree_control'
r 'ls /sys/fs/cgroup/user.slice/ | grep -E "^(memory.reclaim|memory.current|cgroup.freeze)$"'
r 'echo "bu terminal: $(cat /proc/self/cgroup)"'
for name in xfce4-terminal mousepad firefox-esr firefox thunar; do
  pid=$(pgrep -n -x -u "$USER" "$name")
  [ -n "$pid" ] && echo "$name (pid $pid): $(cat /proc/$pid/cgroup)"
done

b "6. zram çekirdek seçenekleri"
r 'grep -E "CONFIG_ZRAM|CONFIG_CRYPTO_(LZ4|ZSTD)=" /boot/config-$(uname -r)'
r 'modinfo -F filename zram'

b "7. Bağımlılık paketleri"
r 'apt-cache policy python3-gi gir1.2-gtk-3.0 gir1.2-wnck-3.0 polkitd pkexec policykit-1 stress-ng | grep -E "^[a-z0-9]|Installed|Candidate|Kurulu|Aday"'

b "8. Ses sunucusu"
r 'pactl info | grep -E "Server Name|Sunucu Adı"'

b "9. Donanım"
r 'lscpu | grep -E "Model name|Model adı|^CPU\(s\)"'
r 'lsblk -d -o NAME,ROTA,SIZE,MODEL'

if [ "$1" = "--zram" ]; then
  b "10. zram özellikleri (geçici yükleme)"
  CREATED=0
  if [ -e /sys/block/zram0 ]; then
    echo "zram0 zaten var; mevcut aygıt okunuyor, hiçbir şey değiştirilmiyor."
  elif sudo modprobe zram; then
    CREATED=1
  fi
  r 'cat /sys/block/zram0/comp_algorithm'
  r 'ls /sys/block/zram0/'
  if [ "$CREATED" = 1 ]; then
    sudo modprobe -r zram && echo "zram modülü yeniden kaldırıldı."
  fi
fi

echo
echo "Çıktı kaydedildi: $OUT"
