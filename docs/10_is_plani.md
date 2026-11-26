# 10 · İş Planı (26 Kasım – 7 Aralık 2026)

**Varsayım:** Kodun büyük kısmı henüz yazılmadı. Daha ilerideyseniz günleri kaydırın, kapsamı değil.

**Kısaltmalar:** **F** = Muhammed Furkan Kaya, **M** = Melih Türker.

**Günlük ritim:**
- **Sabah:** 10 dakikalık görüşme. Dün ne bitti, bugün ne bitecek, engel var mı?
- **Akşam:** Birleştirme (`main` dalına), [checklist](09_checklist.md) ve [bulgular](08_riskler_ve_bulgular.md) güncellemesi.

## Takvim

| Gün | Tarih | F (servis) | M (ajan, arayüz, ölçüm) | Günün sonunda olması gereken |
|---|---|---|---|---|
| 1 | 26 Kas Per | TK-02 deneyi (06-7.1), PSI deneyi (06-7.2); depo iskeleti, Makefile | Paket kontrolü (R-13), libwnck deneyi (06-7.3), `pactl` deneyi; "temiz-kurulum" görüntüsü | Depo hazır; B-08, B-11, B-12, B-13 dolu; TK-02 kararı verildi |
| 2 | 27 Kas Cum | `main.c` (signalfd, timerfd, poll), `config.c`, `psi.c`, `zram.c`; **yoklayıcı `probe.c`** | Ajan: pencere/odak/ses → soket (sahte sunucuya karşı `socat -v UNIX-LISTEN:...`); `collect.py` | Servis zram kurup PSI olaylarını günlüğe yazıyor; ajan mesajları görünüyor |
| 3 | 28 Kas Cmt | `units.c` (adopt, freeze, thaw, thaw_all), `cgroupfs.c`, süreç ağacı | `tab.html`, `load.sh`; ölçüm ayarlı sanal makine ve "olcum-temel" görüntüsü | Komutla bir uygulama gruba alınıp dondurulabiliyor |
| 4 | 29 Kas Paz | `ipc.c` (hello, window, focus, audio, status, subscribe), `apps.c`, `auth.c` (SO_PEERCRED + UID) | **A, B, C ölçümleri** (her biri 3 kez); `analyze.py` ilk hali | Ajan gerçek servise bağlanıyor, uygulamalar otomatik gruplanıyor (G-04, G-21); A–C verisi var |
| 5 | 30 Kas Pzt | `policy.c` kademe 1 ve 2, histerezis; odakla çözme; `test_policy.c` | `zperf-gui` durum ekranı, profil, aç/kapat; `zperfctl` | **Kilometre taşı: uçtan uca çalışan prototip** (yük altında dondurma ve çözme) |
| 6 | 1 Ara Sal | polkit sorgusu (06-6.8), `set_*` komutları; `ExecStopPost`, açılışta thaw-all; ASan | `make install/uninstall`, `make deb`, systemd birimi, autostart, polkit dosyası, bakım betikleri | Paketten kurulabiliyor; T-01 – T-11 ilk tur |
| 7 | 2 Ara Çar | Hata düzeltme; T-07, T-12, T-13, T-30 – T-34 | **D ölçümleri** (3 kez); A–D analizi | **Özellik dondurma:** bu akşamdan sonra yeni özellik yok, yalnızca hata düzeltme |
| 8 | 3 Ara Per | Eşik ayarı gerekiyorsa tek seferlik ayar (07-5.4); rapor: mimari, gerçekleştirim | Gerekirse D tekrar; grafikler; README ilk hali; rapor: test ve değerlendirme | Ölçüm sonuçları kesin; README taslağı |
| 9 | 4 Ara Cum | Rapor: teknik yaklaşım, katkı; M'nin README'si ile T-14 | Rapor: problem, kapsam, grup çalışması; F'nin bölümlerini okuma | Rapor taslağı tamam; T-14 geçti |
| 10 | 5 Ara Cmt | Video kaydı (ortak, 12-1); servis bölümlerini F anlatır | Video kaydı; ajan, arayüz, ölçüm bölümlerini M anlatır; kurgu | Video ham hali |
| 11 | 6 Ara Paz | Raporun son okuması; kaynaklar; yapay zekâ bölümü | Video kurgusu ve YouTube'a yükleme; `VIDEO_LINK.txt`; zip; checklist 4.6 | Teslim paketi hazır |
| 12 | 7 Ara Pzt | **Teslim (sabah).** Gün tampon olarak ayrıldı. | | UZAK'a yüklendi |

## Kilometre taşları ve "yetişmiyor" kararları
| Ne zaman | Kontrol | Geride kalındıysa |
|---|---|---|
| 26 Kas akşamı | TK-02 kararı verildi mi? | Ertesi gün öğlene kadar karar yoksa doğrudan TK-02 alternatifi (cgroupfs) ile devam |
| 29 Kas akşamı | Uygulamalar otomatik gruplanıyor mu? A–C verisi var mı? | Ses algılama düşer (kapsam daraltma 2) |
| 30 Kas akşamı | Uçtan uca prototip çalışıyor mu? | Profiller ikiye iner; arayüz yalnızca durum + `zperfctl` (kapsam daraltma 3 ve 5) |
| 1 Ara akşamı | Paket kuruluyor mu? | `.deb` düşer, `make install` + `install.sh` (kapsam daraltma 4) |
| 2 Ara akşamı | D verisi var mı? | Ölçüm yalnızca C ve D'ye iner (kapsam daraltma 6); rapor yazımı yine 3 Aralık'ta başlar |
| 4 Ara akşamı | Rapor taslağı tamam mı? | Video günü kısaltılır; rapor öncelikli (rapor ve README puanı videodan fazla kalemde geçiyor) |

## Görev dağılımının öneriyle tutarlılığı
Öneride ve bu planda iş bölümü aynıdır:
- **Üye 1 (Furkan):**
  - servis, PSI, cgroup, zram, politika, Unix soket sunucusu, systemd birimi;
  - testler: servis birim testleri, hata durumu testleri, etkileşim yoklayıcısı.
- **Üye 2 (Melih):**
  - ajan, arayüz, IPC protokolünün istemci tarafı, polkit eylemi, paketleme, kurulum belgesi;
  - testler: test düzeneği, yük betiği, karşılaştırmalı ölçümler, veri analizi ve grafikler.

Raporun "Grup çalışması" bölümü bu tablodan ve git geçmişinden yazılır.
