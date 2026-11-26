# Pardus Z-Performans

Düşük RAM'li Pardus cihazlarında bellek baskısına uyarlanan bellek yöneticisi.
Linux çekirdeğinin üç mekanizmasını (zram, PSI, cgroup v2) tek bir politika
altında birleştirir: sistem baskı altına girdiğinde kullanıcının o an
kullanmadığı uygulamaların belleğini zram'e sıkıştırır, gerekirse bunları
dondurur; kullanıcı uygulamaya döndüğünde çözer.

Lisans: GPL-3.0-or-later. Hedef: Pardus 25.0 XFCE (Linux ≥ 6.12).

## Geliştirme durumu

Servis (`zperfd`), odak ajanı, GTK arayüzü, profiller, polkit yetkilendirmesi ve
`.deb` paketi hazır; Pardus 25.2 (çekirdek 6.12) üzerinde uçtan uca test edildi.
Ayrıntılı durum ve kalan işler: [CHECKLIST.md](CHECKLIST.md).

| Bileşen | Yol | Çalışır |
|---|---|---|
| `zperfd` (C, root servis) | `src/zperfd/` | systemd servisi |
| Odak ajanı (Python, libwnck) | `src/agent/` | kullanıcı oturumu, otomatik başlar |
| Denetim arayüzü (GTK 3) | `src/gui/` | kullanıcı, menüden |

## Derleme (Pardus üzerinde)

```sh
sudo apt install build-essential stress-ng gir1.2-wnck-3.0 gir1.2-notify-0.7 \
                 python3-gi-cairo python3-matplotlib
make                                 # -> build/zperfd
make prober                          # -> build/interaction_prober (ölçüm)
```

## Çalıştırma (geliştirme / test)

Servis çekirdek arayüzlerine eriştiği için **root** gerektirir:

```sh
sudo ./build/zperfd --debug
```

Beklenen davranış:

- Açılışta bir zram takas aygıtı kurulur (`/proc/swaps` içinde görünür).
- Her 5 saniyede bir `some`/`full` baskı yüzdeleri loglanır.
- Baskı yükseldiğinde `>> BASKI OLAYI: kademe N` satırları düşer.
- `Ctrl+C` (SIGINT) ile kapandığında zram aygıtı sökülür, sistem temiz kalır.

Baskıyı yapay olarak üretmek için (ayrı bir terminalde):

```sh
sudo apt install stress-ng
stress-ng --vm 2 --vm-bytes 90% --timeout 20s
```

### Seçenekler

| Seçenek | Açıklama | Varsayılan |
|---|---|---|
| `--algorithm <ad>` | zram sıkıştırma algoritması (lz4, zstd…) | `lz4` |
| `--ram-percent <n>` | zram boyutu = RAM'in %n'i | `100` |
| `--swap-prio <n>` | zram takas önceliği | `100` |
| `--no-zram` | zram kurmadan yalnızca PSI izle | — |
| `--debug` | ayrıntılı günlük | — |

## systemd ile (opsiyonel)

```sh
sudo install -m 0755 build/zperfd /usr/sbin/zperfd
sudo install -m 0644 systemd/zperfd.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl start zperfd
journalctl -u zperfd -f
```

## Arayüz ve ajan (geliştirme)

Servis çalışırken (`sudo ./build/zperfd --debug`), **normal kullanıcı** olarak:

```sh
python3 src/agent/main.py                         # odak ajanı (X11)
PYTHONPATH=src/agent python3 src/gui/main.py      # denetim penceresi
```

Profil / aç-kapat / "asla dondurma" komutları polkit
(`org.pardus.zperf.manage`) ister; politika dosyası kurulu değilse bu komutlar
reddedilir. Geliştirmede: `sudo cp polkit/org.pardus.zperf.policy /usr/share/polkit-1/actions/`.
Paket kurulunca arayüz menüde "Pardus Z-Performans" olarak görünür, ajan oturum
açılışında kendiliğinden başlar.

## Testler

```sh
tests/run-unit-tests.sh               # root'suz: psi/applist/ipc
sudo tests/run-unit-tests.sh          # + cgroup/policy (gerçek cgroup v2)
tests/test_agent_reconnect.py         # ajan istemcisi + yeniden bağlanma (sahte servis)
sudo tests/load/pressure-e2e.sh       # uçtan uca: gerçek PSI baskısı + dondurma + yetki
```

`pressure-e2e.sh`, stress-ng'yi bellek sınırlı (`MemoryHigh`) bir systemd
scope'unda çalıştırır; böylece RAM'i bol bir makinede de masaüstünü kilitlemeden
gerçek bellek baskısı oluşur.

## Paket

```sh
sudo apt install debhelper
ln -sfn packaging/debian debian && dpkg-buildpackage -us -uc -b && rm debian
sudo apt install ../zperf_0.1.0_amd64.deb
```
