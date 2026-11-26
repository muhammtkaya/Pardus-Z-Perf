<!--
  Bu dosya, teslim zip'inin kökündeki README.md'nin taslağıdır.
  Köşeli parantezli yerler kod bitince doldurulacak. Her komut, temiz bir Pardus 25 XFCE
  sanal makinesinde gerçekten çalıştırılıp doğrulanmadan teslim edilmemeli (T-14).
-->

# Pardus Z-Performans

Bellek baskısına uyarlanan bellek yöneticisi. Düşük RAM'li Pardus bilgisayarlarında bellek dolduğunda, kullanıcının o an kullanmadığı uygulamaların belleğini sıkıştırılmış RAM'e (zram) iter, gerekirse bu uygulamaları dondurur ve kullanıcı geri döndüğünde onları çözer. Hiçbir uygulamayı kapatmaz.

BİL461 İşletim Sistemleri dersi projesi.
- Muhammed Furkan Kaya (231101014)
- Melih Türker (231101023)

## İçindekiler
1. [Gereksinimler](#gereksinimler)
2. [Kurulum](#kurulum)
3. [Çalıştırma](#çalıştırma)
4. [Kullanım](#kullanım)
5. [Örnek senaryo](#örnek-senaryo)
6. [Testler ve ölçümler](#testler-ve-ölçümler)
7. [Kaldırma](#kaldırma)
8. [Bilinen kısıtlar](#bilinen-kısıtlar)
9. [Bağımlılıklar ve lisanslar](#bağımlılıklar-ve-lisanslar)

## Gereksinimler
| | |
|---|---|
| İşletim sistemi | **Pardus 25 XFCE** (Pardus 25.2 üzerinde test edildi) |
| Çekirdek | Linux 6.12 (Pardus 25 varsayılanı). Gerekenler: zram, PSI, cgroup v2, `memory.reclaim` (≥ 5.19) |
| Masaüstü | XFCE, X11 oturumu (Wayland desteklenmez) |
| Donanım | x86_64; en az 2 GB RAM önerilir |
| Yetki | Kurulum için `sudo` |

Kontrol:
```bash
grep VERSION_ID /etc/os-release
uname -r
echo $XDG_SESSION_TYPE
ls /proc/pressure/memory
```
Beklenen çıktılar sırasıyla:
- `VERSION_ID="25.x"`,
- `6.12…`,
- `x11`,
- dosya yolu.

## Kurulum

### Yol A: Paketten (önerilen)
```bash
sudo apt install ./zperf_[sürüm]_amd64.deb
```
Kurulumdan sonra oturumu kapatıp açın; ajan oturumla birlikte başlar.

### Yol B: Kaynaktan
```bash
sudo apt install -y build-essential pkg-config libsystemd-dev libjson-c-dev \
  python3-gi gir1.2-gtk-3.0 gir1.2-wnck-3.0 pulseaudio-utils
cd source_code
make
sudo make install
sudo systemctl daemon-reload
sudo systemctl enable --now zperfd
```
Ardından oturumu kapatıp açın.

Paketi kaynaktan üretmek için:
```bash
make deb
```

## Çalıştırma
| Ne | Komut |
|---|---|
| Servisin durumu | `systemctl status zperfd` |
| Servis günlüğü (canlı) | `journalctl -fu zperfd` |
| Zram takası | `swapon --show` ve `zramctl` |
| Arayüz | Uygulamalar menüsü → Sistem → **Z-Performans**, ya da `zperf-gui` |
| Komut satırı | `zperfctl status`, `zperfctl profile akilli-tahta`, `zperfctl disable` |
| Ajan (elle) | `zperf-agent &` (normalde otomatik başlar) |

## Kullanım
- **Profiller:**
  - **Dengeli** (varsayılan): genel kullanım.
  - **Akıllı Tahta:** ön plandaki uygulamayı en agresif korur, arka plandakileri daha erken sıkıştırır ve dondurur.
  - **Ofis:** hiçbir uygulamayı dondurmaz, yalnızca arka plandakileri sıkıştırır.
- **Politikayı kapatma:** Arayüzdeki anahtarla. Tüm uygulamalar çözülür; zram açık kalır.
- **"Asla dondurma":** Arayüzdeki uygulama listesinde ilgili kutu işaretlenir. Ses çalan uygulamalar zaten hiç dondurulmaz.
- **Donmuş uygulamaya dönmek:** Pencereye tıklamanız ya da Alt+Tab ile geçmeniz yeterli; uygulama hemen çözülür ve birkaç saniye içinde kullanılabilir olur.

Ayarlar `/etc/zperf/zperf.conf` dosyasında saklanır.

## Örnek senaryo
[07-4 kabul senaryosunun kısa hali; kod bitince komutlarla doldurulacak]
1. **Kurulum:** Arayüzü açın, profili "Akıllı Tahta" yapın.
2. **Arka plan:** LibreOffice'te bir belge açıp birkaç satır yazın (kaydetmeyin). Firefox'ta bir video oynatın.
3. **Yük:** Bellek baskısı oluşturmak için:
   ```bash
   source_code/tools/bench/load.sh --tabs 15
   ```
4. **Gözlem:** Arayüzde kademe yükselir, LibreOffice önce sıkıştırılır, sonra "donuk" olur ve bir bildirim görünür. Video akmaya devam eder.
5. **Geri dönüş:** LibreOffice'e geçin. Uygulama çözülür ve yazdığınız metin yerindedir.

## Testler ve ölçümler
```bash
cd source_code
make test
```
Birim testlerini çalıştırır.

Entegrasyon testleri ve A–D karşılaştırmalı ölçümler için bkz. `source_code/tools/bench/README.md` ve teknik rapor bölüm 7. Özet sonuçlar: [tablo kod bitince eklenecek].

## Kaldırma
```bash
sudo apt remove zperf
```
Kaynaktan kurulduysa:
```bash
sudo make uninstall
```
Kaldırma sırasında zram takası (yeterli boş bellek varsa) kapatılır ve `vm.swappiness` eski değerine döner.

## Bilinen kısıtlar
[Kod bitince 08-bulgular ve test sonuçlarından güncellenecek]
- Yalnızca XFCE ve X11 desteklenir; GNOME ve Wayland desteklenmez.
- Tarayıcılar tek uygulama olarak yönetilir; sekme bazında yönetim yoktur.
- Ajan başlamadan önce açılmış uygulamaların o ana kadar ayırdığı bellek, sıkıştırmanın kapsamı dışında kalır.
- Sıkıştırma algoritması değişikliği bir sonraki açılışta geçerli olur.
- PID bildirmeyen pencereler yönetilemez: [liste].
- Ölçümler sanal makinede yapılmıştır (2 GB RAM, sınırlı CPU ve disk).

## Bağımlılıklar ve lisanslar
Projenin kodu **GPL-3.0** lisanslıdır (bkz. `source_code/LICENSE`).

| Bileşen | Kullanım | Lisans |
|---|---|---|
| libsystemd (sd-bus) | systemd ve polkit ile D-Bus iletişimi | LGPL-2.1+ |
| json-c | IPC mesajları | MIT |
| GTK 3 | Arayüz | LGPL-2.0+ / LGPL-2.1+ |
| PyGObject | Python'dan GTK ve libwnck | LGPL-2.1+ |
| libwnck 3 | Odaktaki pencere | LGPL-2.0+ |
| polkit | Yetkilendirme | LGPL-2.0+ |
| pulseaudio-utils (`pactl`) | Ses çalan uygulamaları bulma | [doğrulanacak] |
| stress-ng | Yalnızca testlerde yük | GPL-2.0+ |

`json-c` ve `pulseaudio-utils` lisansları Debian telif dosyalarından doğrulanıp tabloya yazılmalı.

Tasarımda fikir kaynağı olarak yararlanılan ama kodu kullanılmayan projeler: Meta senpai/TMO, Android lmkd.
