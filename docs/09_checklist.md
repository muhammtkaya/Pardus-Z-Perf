# 09 · Checklist

Her akşam bu listeye bakılır. İşaretlenen her madde için kanıt (commit, test kimliği, dosya) yanına yazılır.

## 1. Başlangıç (26 Kasım)
- [+] Bu dokümanlar iki üye tarafından okundu; sorular not edildi
- [+] Git deposu oluşturuldu (GitHub, şimdilik özel); `.gitignore`, `LICENSE` (GPL-3.0), boş dizin yapısı (06-2)
- [ ] Geliştirme paketleri kuruldu; `apt-cache policy` ile paket adları doğrulandı (R-13)
- [ ] "temiz-kurulum" anlık görüntüsü var
- [ ] TK-02 deneyi (06-7.1) yapıldı; B-08 ve B-11 dolduruldu; TK-02 kabul edildi veya alternatife geçildi
- [ ] PSI tetikleyici deneyi (06-7.2) ve libwnck deneyi (06-7.3) yapıldı; B-12 ve B-13 dolduruldu

## 2. Geliştirme (gereksinim bazında)
**Servis (Furkan)**
- [ ] G-01 systemd birimi, `Type=notify`, `Restart=on-failure`
- [ ] G-02 zram kurulumu; var olanı kullanma
- [ ] G-03 PSI tetikleyicileri ve `poll()` döngüsü
- [ ] G-04 erken sahiplenme (geçici scope, süreç ağacı)
- [ ] G-05 kademe 1
- [ ] G-06 kademe 2, kurban seçimi, 5 sn aralık
- [ ] G-07 odakla çözme < 1 sn
- [ ] G-08 SIGTERM, `ExecStopPost`, açılışta thaw-all
- [ ] G-09 dışlama listesi ve "asla dondurma"
- [ ] G-10 histerezis
- [ ] G-12, G-13 yapılandırma ve profiller
- [ ] G-14 `status` komutu
- [ ] G-15 `EVT` günlük satırları
- [ ] G-52 SO_PEERCRED, UID denetimi, polkit
- [ ] Birim testleri U-01 – U-09 geçiyor (`make test`)
- [ ] `make SAN=1` ile ASan/UBSan uyarısı yok

**Ajan, arayüz, paket (Melih)**
- [ ] G-20 autostart
- [ ] G-21 pencere bildirimi (açılışta tam liste)
- [ ] G-22 odak bildirimi
- [ ] G-23 ses algılama
- [ ] G-24 bildirimler
- [ ] G-25 yeniden bağlanma
- [ ] G-30 durum ekranı
- [ ] G-31 profil seçimi
- [ ] G-32 aç/kapat
- [ ] G-33 "asla dondurma"
- [ ] `zperfctl` (status, profile, enable/disable)
- [ ] `make install`, `make uninstall`
- [ ] `make deb`; `postinst`, `prerm`, `postrm` (G-55)

**Ölçüm araçları**
- [ ] G-40 yoklayıcı (Furkan)
- [ ] G-41 toplayıcı (Melih)
- [ ] G-42 yük betiği ve `tab.html` (Melih)
- [ ] G-43 analiz betiği (Melih)

## 3. Test ve değerlendirme
- [ ] Entegrasyon testleri T-01 – T-15 çalıştırıldı ve `results/test_sonuclari.md` dolduruldu
- [ ] Hata durumu testleri T-30 – T-34
- [ ] Kabul senaryosu (07-4) D ve C yapılandırmalarında oynandı
- [ ] "olcum-temel" anlık görüntüsü hazırlandı (07-1)
- [ ] A, B, C ölçümleri (her biri ≥ 3)
- [ ] D ölçümleri (≥ 3)
- [ ] Varsa eşik ayarı sonrası D tekrar ölçüldü; ilk sonuç da saklandı
- [ ] M1–M6 tablosu ve grafikler üretildi (`tools/bench/analyze.py`)
- [ ] Hedeflerin tutup tutmadığı ve nedenleri yazıldı
- [ ] T-14: kodu yazmayan üye temiz makinede README ile kurdu

## 4. Aşama 2 teslim şartları

### 4.1 Projenin geliştirilmesi (Aşama 2, bölüm 1)
- [ ] Temel işlevler çalışıyor (önerideki zorunlu işlevler 1–6; bkz. 02 izlenebilirlik tablosu)
- [ ] Pardus üzerinde test edildi (sürüm, çekirdek ve masaüstü raporda yazılı)
- [ ] Temel kullanım senaryosu uygulanabilir (07-4)
- [ ] Bilinen hatalar, eksiklikler ve kısıtlar README'de ve raporda açıkça yazılı
- [ ] Üyelerin görev ve sorumlulukları somut olarak raporda yazılı
- [ ] Her üye projenin tamamını ve kendi katkısını açıklayabiliyor (12-3 soru listesi çalışıldı)

### 4.2 Kaynak kod (2.1)
- [ ] Çalışma için gereken tüm kaynak kod `source_code/` içinde
- [ ] Yapılandırma dosyaları ve kaynaklar dahil: `config/`, `systemd/`, `polkit/`, `xdg/`, `packaging/`
- [ ] Üçüncü taraf kütüphaneler ve bağımlılıklar README'de lisanslarıyla listeli
- [ ] Geçici dosya, derleme çıktısı (`*.o`, ikili, `.deb`, `build/`), `__pycache__`, büyük veri dosyası **yok**
- [ ] `.git` klasörü zip'e konmadı (veya bilinçli olarak konduysa boyutu küçük)

### 4.3 README.md (2.2)
- [ ] Hedef Pardus sürümü ve sistem gereksinimleri
- [ ] Gerekli yazılım ve kütüphaneler
- [ ] Kurulum adımları
- [ ] Çalıştırma komutları
- [ ] Temel işlevlerin kullanımı
- [ ] Örnek kullanım veya test senaryosu
- [ ] Projeyi daha önce görmemiş biri takip edip çalıştırabiliyor (T-14)

### 4.4 Teknik rapor (2.3)
Başlıkların tamamı ve kaynakları için bkz. [Teknik rapor planı](11_teknik_rapor_plani.md).
- [ ] Proje başlığı ve grup üyeleri (ad, soyad, öğrenci no)
- [ ] Problem ve amaç
- [ ] Kapsam ve katkı (mevcut çözümlerden fark)
- [ ] Teknik yaklaşım (diller, araçlar, kütüphaneler, tercihler)
- [ ] Sistem mimarisi (şema dahil)
- [ ] Gerçekleştirim (kararlar, detaylar, karşılaşılan problemler)
- [ ] Test ve değerlendirme (testler, senaryolar, sonuçlar)
- [ ] Grup çalışması (her üyenin geliştirme ve test görevleri)
- [ ] Sonuç ve kısıtlar (durum, eksikler, gelecek çalışmalar)
- [ ] Kaynaklar ve yapay zekâ kullanımı
- [ ] PDF olarak `technical_report.pdf`; içindekiler sayfa numaraları güncel

### 4.5 Demo videosu (2.4)
- [ ] 8–10 dakika (süre kontrol edildi)
- [ ] Yalnızca slayt değil; yazılımın çalışması gösteriliyor
- [ ] Gerçek kullanım senaryosu üzerinden anlatım
- [ ] Temel işlevler ve katkı gösteriliyor
- [ ] YouTube'a "liste dışı" (unlisted) olarak yüklendi; çıkış yapılmış bir tarayıcıda açılıyor
- [ ] Bağlantı `VIDEO_LINK.txt` içinde

### 4.6 Zip yapısı (bölüm 3)
```
Pardus_Z_Performans.zip
├── source_code/          (git deposunun içeriği)
├── README.md
├── technical_report.pdf
└── VIDEO_LINK.txt
```
- [ ] Zip açıldığında yapı yukarıdaki gibi; dosya adları anlaşılır
- [ ] Zip temiz bir klasöre açılıp README'deki kurulum oradan denendi
- [ ] UZAK sistemine yüklendi; yükleme onayı alındı (ekran görüntüsü saklandı)

### 4.7 Özgünlük ve yapay zekâ (bölüm 6)
- [ ] Açık kaynak bileşenler ve lisansları raporda ve README'de
- [ ] Grubun özgün katkısı açıkça ayrılmış ([Net katkı](04_net_katki.md))
- [ ] Yapay zekâ kullanımı amaçlarıyla raporda ([kayıt](14_yapay_zeka_kullanim_kaydi.md))
- [ ] Her üye teslim edilen kodun tamamını açıklayabiliyor

## 5. Sözlü sunum
- [ ] Sunum akışı hazır (12-2)
- [ ] Canlı demo için sanal makine hazır; yedek olarak video var
- [ ] Soru listesi (12-3) iki üye tarafından birlikte çalışıldı
- [ ] Sunum tarih ve saati takvimde
