# 15 · Kod İskeletinin Durumu

İskelet 26 Kasım'da `Real` dalına eklendi. Bu tablo her `TODO` kapandıkça güncellenir; tablo boşaldığında kod tamamdır.

**İskeletin ilkesi:**
- **Hazır olan:** Altyapı (derleme, ana döngü, soket, yardımcılar, kurulum dosyaları).
- **Size bırakılan:** Projenin asıl işi (politika, systemd, zram, polkit, ajan ve arayüz mantığı, ölçüm araçları).

Sözlü sunumda her üye kendi bileşenini satır satır açıklayabilmelidir.

## Nasıl başlanır
```bash
git clone -b Real https://github.com/muhammtkaya/Pardus-Z-Perf.git
cd Pardus-Z-Perf/source_code
sudo apt install -y build-essential pkg-config libsystemd-dev libjson-c-dev
make
make test
```
- **`make test`:** `test_config` geçer. `test_policy` politika yazılana kadar **kalır**; bu bilerek böyle. Testler davranışı önceden tanımlıyor.
- **Servisi elle denemek:** Kurmadan, root olarak ön planda çalıştırın. Yapılandırma dosyası yoksa varsayılanlar kullanılır.
  ```bash
  sudo ./daemon/zperfd -d -c config/zperf.conf
  ```
- **Bağlantıyı kontrol etmek:** Başka bir terminalde:
  ```bash
  ./cli/zperfctl status
  ```

## Kontrol durumu
| Kontrol | Sonuç |
|---|---|
| Tüm C modülleri `-Wall -Wextra -Wshadow` ile derleniyor | Uyarısız (WSL Ubuntu, gcc 15). **sd-bus ve json-c gerçek başlıklarıyla henüz derlenmedi**; Pardus'ta ilk `make` ile doğrulanacak |
| `test_config` | 31/31 geçti |
| `test_policy` | Politika TODO; 2/8 test geçiyor (beklenen) |
| Kabuk betikleri (`sh -n` / `bash -n`) | Geçti |
| Python dosyaları (3.11+ sözdizimi) | Geçti; `zperfctl` Linux'ta servis yokken doğru hata veriyor |
| Pardus üzerinde çalıştırma | **Yapılmadı** |

## Dosya bazında durum
| Dosya | Hazır | TODO | Sorumlu | Gereksinim |
|---|---|---|---|---|
| `Makefile` | Hepsi | — | — | — |
| `daemon/src/main.c` | Döngü, sinyaller, zamanlayıcı, PSI tetikleyicileri, soket, sd_notify | `on_tick`: uygulama bellek okumaları; `apply_action`: reclaim ve freeze; profil değişiminde swappiness | Furkan | G-05, G-06, G-12, G-15 |
| `daemon/src/util.*`, `log.h` | Hepsi | — | — | — |
| `daemon/src/config.*` | Hepsi (profil tablosu, okuma/yazma) | Profil değerleri ölçümle ayarlanacak | İkisi | G-12, G-13 |
| `daemon/src/psi.*` | Hepsi | — | — | G-03 |
| `daemon/src/cgroupfs.*` | Hepsi | — | — | G-05 |
| `daemon/src/apps.*` | Tablo, scope adı, sistem bileşenleri | `apps_find_pid`: alt süreç PID'i ile arama | Furkan | G-07, G-09 |
| `daemon/src/zram.*` | `zram_active` | `zram_setup`, `zram_stats` | Furkan | G-02, G-14 |
| `daemon/src/units.*` | Arayüz | Hepsi (StartTransientUnit, Freeze, Thaw, thaw_all) | Furkan | G-04, G-07, G-08 |
| `daemon/src/policy.*` | Arayüz, `policy_init` | `policy_step` | Furkan | G-05, G-06, G-10 |
| `daemon/src/auth.*` | `auth_owns_pid` | `auth_polkit_check` | Furkan | G-52 |
| `daemon/src/ipc.*` | Soket, bağlantı kabulü, satırlara bölme, 64 KB sınırı, `hello`, `status`, `subscribe`, yetki denetimi çağrısı | `window`, `focus`, `audio`, `set_*` komutlarının içi; `status`'a mem/zram | Furkan | G-04, G-07, G-14, G-21 – G-23, G-31 – G-33 |
| `daemon/tests/*` | `test_config`, `test_policy` (U-01 – U-09) | — | — | — |
| `agent/zperf-agent` | Bağlantı, yeniden bağlanma, satır okuma | Pencere, odak, ses bildirimi; bildirimler | Melih | G-21 – G-24 |
| `gui/zperf-gui` | Pencere düzeni, istek, yenileme döngüsü | Durumu ekrana yazma; profil, aç/kapat, asla dondurma | Melih | G-30 – G-33 |
| `cli/zperfctl` | Hepsi | — | Melih | — |
| `systemd/`, `polkit/`, `xdg/`, `gui/*.desktop`, `config/` | Hepsi | — | — | G-01, G-20, G-52 |
| `scripts/zperf-thaw-all` | Hepsi | T-07 ile doğrulanacak | — | G-08 |
| `packaging/DEBIAN/*` | control, postinst, prerm | `postrm`: swappiness ve zram'in geri alınması; `control`: Maintainer e-postası | Melih | G-53, G-55 |
| `tools/probe/probe.c` | Döngü iskeleti | Bellek ayırma, dokunma, CSV | Furkan | G-40 |
| `tools/bench/tab.html` | Hepsi | Sekme başına MB, ölçümle sabitlenecek | — | G-42 |
| `tools/bench/collect.py` | İskelet | Tüm okuma fonksiyonları | Melih | G-41 |
| `tools/bench/load.sh` | Argümanlar, çıktı dizini | Adımların hepsi | Melih | G-42 |
| `tools/bench/analyze.py` | İskelet | M1–M6, tablo, grafikler | Melih | G-43 |

## Bilinen eksikler
- **`LICENSE` yok.** Pardus'ta şu komutla ekleyin:
  ```bash
  cp /usr/share/common-licenses/GPL-3 source_code/LICENSE
  ```
- **`packaging/DEBIAN/control`:** Maintainer e-postası `TODO`.
- **`Depends` satırı:** `libjson-c5` paket adı Pardus'ta `apt-cache policy` ile doğrulanmalı.
- **Ajan bildirimleri:** `gir1.2-notify-0.7` paketi henüz bağımlılık listesinde değil. Bildirimler `notify-send` ile yapılacaksa `libnotify-bin` eklenmeli.
