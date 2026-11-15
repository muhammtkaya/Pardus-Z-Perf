# Pardus Z-Performans

Düşük RAM'li Pardus cihazlarında bellek baskısına uyarlanan bellek yöneticisi.
Linux çekirdeğinin üç mekanizmasını (zram, PSI, cgroup v2) tek bir politika
altında birleştirir: sistem baskı altına girdiğinde kullanıcının o an
kullanmadığı uygulamaların belleğini zram'e sıkıştırır, gerekirse bunları
dondurur; kullanıcı uygulamaya döndüğünde çözer.

Lisans: GPL-3.0-or-later. Hedef: Pardus 25.0 XFCE (Linux ≥ 6.12).

## Geliştirme durumu

Bu depo parçalar hâlinde geliştirilmektedir. Şu an tamamlanan:

- **Aşama 1 — `zperfd` iskeleti** (bu sürüm): zram kurulumu, PSI olay döngüsü,
  günlük kaydı ve temiz kapanış. Baskı olayları algılanır ve loglanır; henüz
  **tepki verilmez** (cgroup taşıma, dondurma, `memory.reclaim` ve politika
  motoru sonraki aşamada eklenecek).

## Önemli: nerede derlenir?

`zperfd` C servisi yalnızca **Linux/Pardus** üzerinde derlenip çalışır
(sysfs, `swapon`, PSI, cgroup v2 Windows'ta yoktur). Önerilen akış:

1. Pardus 25.0 XFCE'yi bir sanal makineye kur (VirtualBox, 2 GB RAM, 2 vCPU).
2. Bu depoyu sanal makineye kopyala.
3. Aşağıdaki adımlarla derleyip çalıştır.

## Derleme (Pardus üzerinde)

```sh
sudo apt install build-essential     # gcc, make
make                                 # -> build/zperfd
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

## Doğrulama kontrol listesi (Aşama 1)

- [ ] `make` hatasız derliyor.
- [ ] `sudo ./build/zperfd --debug` zram aygıtını kuruyor (`cat /proc/swaps`).
- [ ] `stress-ng` ile baskı üretince `BASKI OLAYI` logları görünüyor.
- [ ] `Ctrl+C` sonrası `cat /proc/swaps` zram satırını artık göstermiyor.
