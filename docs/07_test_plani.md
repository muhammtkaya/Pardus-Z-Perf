# 07 · Test Planı

## 1. Test ortamı
| Öğe | Değer |
|---|---|
| Sanallaştırma | VirtualBox, makine adı `Pardus25-XFCE` |
| İşletim sistemi | Pardus 25 XFCE (`VERSION_ID=25.2`), çekirdek 6.12.111+deb13-amd64, X11 |
| Geliştirme ayarları | 4 GB RAM, 2 sanal çekirdek, sınır yok |
| **Ölçüm ayarları** | **2 GB RAM, 2 sanal çekirdek, CPU kullanım sınırı %40, disk bant genişliği 20 MB/s** |
| Başlangıç durumu | Her ölçüm çalıştırması "olcum-temel" anlık görüntüsünden başlar |
| Ana bilgisayar | Intel i5-12450H. Ölçüm sırasında ana bilgisayarda başka ağır iş çalıştırılmaz; laptop prize takılı olur. |

"olcum-temel" anlık görüntüsü şöyle hazırlanır:
1. Temiz kurulumdan sonra test araçları kurulur.
2. Z-Performans kurulur ama servisi **devre dışı** bırakılır (`systemctl disable zperfd`).
3. zram-tools kurulur ama **devre dışı** bırakılır (`systemctl disable zramswap`).
4. Makine kapatılıp anlık görüntü alınır.

Bu sayede A–D yapılandırmaları aynı görüntüden yalnızca birkaç komutla elde edilir (bölüm 5).

## 2. Birim testleri (`make test`)
| Kimlik | Ne test edilir | Beklenen | Gereksinim |
|---|---|---|---|
| U-01 | `policy_step`: K1 ateşlenince seviye 0 → 1 | Seviye 1 ve odakta olmayan her uygulama için bir `ACT_RECLAIM` | G-05 |
| U-02 | K2 ateşlenince seviye 2 ve kurban seçimi | En büyük `mem` değerine sahip, boşta süresi yeterli aday seçilir | G-06 |
| U-03 | Odaktaki uygulama | Hiçbir eylemde yer almaz | G-06 |
| U-04 | Asla dondurma, ses, dışlama bayrakları | `ACT_FREEZE` üretilmez | G-09 |
| U-05 | Dondurma aralığı | 5 saniye dolmadan ikinci `ACT_FREEZE` üretilmez | G-06 |
| U-06 | Histerezis | avg10 eşiğin altına iner ama 10 sn dolmazsa seviye düşmez; dolunca düşer | G-10 |
| U-07 | "Ofis" profili | Seviye 2'de bile `ACT_FREEZE` üretilmez | G-13 |
| U-08 | `config` ayrıştırma | Bilinmeyen anahtar uyarıyla atlanır; geçersiz profil adı varsayılana döner | G-12 |
| U-09 | Scope adı temizleme | `"soffice.bin"` → `zperf-soffice-bin-<pid>.scope`; 32 karakter sınırı | Güvenlik |

## 3. Entegrasyon testleri (sanal makinede, geliştirme ayarlarıyla)
Her test için sonuç tablosu bölüm 7'deki şablonla doldurulur. Komutlar örnektir; tam betikler `tools/tests/` altında tutulur.

| Kimlik | Amaç | Adımlar | Geçme ölçütü | Gereksinim |
|---|---|---|---|---|
| T-01 | Kurulum ve zram | Paketi kur, yeniden başlat. `systemctl status zperfd`, `swapon --show`, `zramctl` | Servis `active (running)`; `/dev/zram0` lz4, öncelik 100; disk takası da duruyor (öncelik -2) | G-01, G-02 |
| T-02 | PSI tetikleyicisi | `journalctl -fu zperfd` açıkken `stress-ng --vm 2 --vm-bytes 90% -t 60s` | 2 sn içinde `EVT level from=0 to=1` satırı; boşta servis CPU'su ≈ %0 | G-03 |
| T-03 | Erken sahiplenme | Mousepad ve Firefox aç; `cat /proc/<pid>/cgroup` | 2 sn içinde `zperf-mousepad-<pid>.scope`; Firefox'un tüm içerik süreçleri aynı scope'ta | G-04, G-21 |
| T-04 | Kademe 1 | Firefox'ta 6 sentetik sekme; Mousepad arka planda; yük betiğiyle baskı | Günlükte arka plan uygulamaları için `reclaim` olayları; odaktakine hiç yok; `memory.swap.current` artar | G-05, G-15 |
| T-05 | Kademe 2 | T-04'e devam, baskıyı artır | En uzun süre boşta kalan büyük uygulama dondurulur (`FreezerState=frozen`), belleği düşer; hiçbir süreç öldürülmez | G-06, G-11 |
| T-06 | Odakla çözme | Donmuş uygulamaya Alt+Tab ile geç | `EVT thawed ... thaw_ms` < 1000; uygulama birkaç saniyede kullanılabilir | G-07, G-22 |
| T-07 | Güvenli kapanış ve çökme | Bir uygulama donukken: (a) `systemctl stop zperfd`, (b) `kill -9 $(pidof zperfd)`, (c) ajanı öldür, yeniden başlat | Üç durumda da donuk scope kalmaz; (b)'de servis 1 sn içinde yeniden başlar; (c)'de ajan bağlanınca pencere listesi yeniden gelir | G-08, G-25 |
| T-08 | Dışlamalar | VLC/Parole ile ses çal; bir uygulamayı "asla dondurma" yap; baskı oluştur | Bu uygulamalar ve masaüstü bileşenleri asla `frozen` olmaz | G-09, G-23, G-33 |
| T-09 | Profil ve açma/kapatma | Arayüzden profil değiştir; politikayı kapat | Günlükte yeni eşikler; kapatınca tüm scope'lar çözülür; algoritma değişikliği uyarısı görünür | G-12, G-13, G-31, G-32 |
| T-10 | Durum bilgisi | Arayüzü aç; `zperfctl status` | Değerler `/proc/pressure/memory` ve `mm_stat` ile tutarlı; 1 sn'de güncellenir | G-14, G-30 |
| T-11 | Otomatik başlama ve bildirim | Oturumu kapat ve aç | Ajan çalışıyor (`pgrep -f zperf-agent`); dondurmada bildirim görünüyor | G-20, G-24 |
| T-12 | Kaynak kullanımı | 10 dk boşta, sonra 10 dk yükte `pidstat` veya `ps -o rss,pcpu` | Boşta servis < %1 CPU, < 10 MB RSS | G-51 |
| T-13 | Güvenlik | İkinci bir kullanıcı (`deneme`) oluştur; onun oturumundan `socat` ile birinci kullanıcının PID'i için `focus`/`window` ve `set_profile` gönder | `window` yok sayılır; `set_profile` etkin olmayan oturumdan reddedilir | G-52 |
| T-14 | README ile temiz kurulum | **Kodu yazmayan üye**, "temiz-kurulum" görüntüsünde yalnızca README'yi izler; sonra paketi kaldırır | 15 dk içinde çalışır; kaldırınca zram kapanır, `swappiness` 60'a döner, birimler silinir | G-53, G-55 |
| T-15 | logind ve uygulama davranışı | Scope'a alınmış Mousepad ve Firefox'ta kaydet/aç pencereleri, yazdır penceresi, Firefox'ta dosya indirme | Hepsi çalışır; çalışmayan varsa B-08'e yazılır | TK-02 |

## 4. Kullanım senaryosu testi (kabul testi)
Öneride verilen senaryonun elle oynanması; videoda da bu gösterilir.
1. Ölçüm ayarlarıyla (2 GB RAM) makineyi açın. Servis açık, profil "Akıllı Tahta".
2. Arka planda LibreOffice'te bir belge açın ve birkaç satır yazın, kaydetmeyin. Bir PDF ders kitabı açın.
3. Firefox'ta yerel bir eğitim videosunu ön planda oynatın. Yük betiğiyle sentetik sekmeleri açmaya başlayın.
4. **Gözlem:**
   - Video takılıyor mu?
   - Arayüzde LibreOffice önce sıkıştırılıyor, sonra donuyor mu?
   - Bildirim geliyor mu?
5. LibreOffice'e geçin.
   - **Geçme ölçütü:** Birkaç saniyede kullanılabilir oluyor ve kaydedilmemiş metin yerinde.
6. Aynı adımları **C yapılandırmasında** (zram-tools) tekrarlayın ve farkı not edin.

## 5. Performans testi: A–D karşılaştırması

### 5.1 Yapılandırmalar ("olcum-temel" görüntüsünden)
| | Yapılandırma | Kurulum komutları (her çalıştırmadan önce) |
|---|---|---|
| A | Takas yok | `sudo swapoff -a` |
| B | Disk takası | Hiçbir şey (varsayılan: `/dev/sda5`, swappiness 60) |
| C | zram-tools varsayılanı | `sudo systemctl start zramswap` (paketin varsayılan ayarları değiştirilmez) |
| D | Z-Performans | `sudo systemctl start zperfd`, profil **Akıllı Tahta** |

- **C ve D'de disk takası:** Disk takası kapatılmaz; iki yapılandırmada da zram yüksek, disk düşük öncelikle durur. Böylece fark yalnızca politikadan gelir.
- **Ortak kurulum:** Her çalıştırmada etkileşim yoklayıcısı ve veri toplayıcı aynı biçimde başlatılır.

### 5.2 Yük
`tools/bench/load.sh` her çalıştırmada aynı sırayı uygular:
1. `t=0`: Veri toplayıcı ve yoklayıcı başlar.
2. `t=10 sn`: LibreOffice Writer bir belge açar. Mousepad 20 MB'lık metni açar.
3. `t=30 sn`: Firefox'ta yerel bir video (720p, döngü) açılır; bu pencere **odakta kalır**.
4. `t=40 sn`'den itibaren her 20 saniyede bir yeni sentetik sekme açılır: `firefox-esr --new-tab file:///…/tab.html?mb=120`.
   - `tab.html` her sekmede belirtilen kadar bellek ayırır (rastgele baytlarla dolu dizi; sıkıştırma oranı gerçekçi olsun diye yarısı rastgele, yarısı tekrarlayan desen).
   - Saniyede bir bu belleğin %5'ine dokunur; bu, arka planda "yaşayan" sekmeleri temsil eder.
5. **Bitiş:** 25. sekmede ya da yoklayıcı tek seferde 30 saniyeyi aşan bir takılma görürse durur. Sistem toparlanmazsa en fazla 15 dakika sonra durur.

`tab.html`'deki bellek miktarı ilk denemede, B yapılandırmasında yaklaşık 10. sekmede thrashing başlayacak şekilde ayarlanır. Bu değer **sonra sabitlenir** ve raporda yazılır.

### 5.3 Ölçümler
| Kod | Ölçüm | Kaynak | Hesap |
|---|---|---|---|
| M1 | Ön plandaki takılma süresi | Yoklayıcı CSV'si (`ts, gecikme_ms`) | 200 ms'yi aşan gecikmelerin toplamı (sn) |
| M2 | Ağır bellek baskısı süresi | Toplayıcı: `/proc/pressure/memory` | `full avg10 > 10` olan saniye sayısı |
| M3 | Tepkisizliğe kadar açılabilen sekme sayısı | Yük betiği günlüğü + yoklayıcı | Tek seferde 2 sn'yi aşan ilk takılmadan önceki sekme sayısı |
| M4 | OOM killer ile kapanan süreç sayısı | `journalctl -k` | `Out of memory: Killed process` satır sayısı |
| M5 | Maliyet | Toplayıcı: `/proc/stat` CPU, `/proc/vmstat` `pswpin`/`pswpout`, zram `mm_stat` | Ortalama CPU %, disk takas sayfa sayısı, sıkıştırma oranı |
| M6 | Donmuş uygulamanın dönüş süresi | Servis günlüğü `thaw_ms` + elle zamanlama | Odak olayından çözülmeye kadar (ms); ayrıca kullanılabilir hale gelene kadar elle kronometre (yalnızca D) |

- **Yoklayıcının kendisi:** Odakta değildir ve hiçbir yapılandırmada dondurulmaz (pencere açmaz, ajan bildirmez). Ölçtüğü şey "bir sürecin bellek baskısı altında ne kadar geciktiği"dir.
- **M1 ve M3'ün sınırı:** Bu ölçümler videonun kendisinin takılmasını doğrudan ölçmez. Bu sınırlılık raporda belirtilir.

### 5.4 Tekrar ve istatistik
- **Tekrar:** Her yapılandırma **en az 3 kez**, süre kalırsa 5 kez. Sıra karıştırılır (ABCD, BCDA, CDAB...); böylece sanal makinenin ısınması veya ana bilgisayardaki değişimler tek bir yapılandırmayı etkilemez.
- **Raporlama:** Ortalama ± standart sapma, ayrıca her çalıştırmanın ham değeri tabloda.
- **Hedeflerin değerlendirilmesi** (D'nin C'ye göre):
  - M1'de en az %30 azalma,
  - M3'te en az %20 artış,
  - M4 = 0,
  - M6 < 3 sn.

  Bir hedef tutmazsa sonuç **olduğu gibi** raporlanır ve nedeni tartışılır. Ölçümler, hedef tutsun diye tekrarlanmaz.
- **Eşik ayarı:** Profil eşikleri ölçümden sonra en fazla bir kez değiştirilir. Değiştirilirse D baştan ölçülür ve ilk sonuç da raporda verilir.

### 5.5 Geçerliliği tehdit eden etkenler (raporda yazılacak)
- Sanal makine gerçek eski donanım değildir: disk sınırı hızı taklit eder ama HDD'nin arama gecikmesini taklit etmez.
- Sentetik sekmeler gerçek web sayfalarının bellek erişim desenini birebir yansıtmaz.
- 3 tekrar istatistiksel olarak zayıftır; farklar küçükse "anlamlı fark" iddiası yapılmaz.
- Ana bilgisayarın arka plan yükü ölçümleri etkileyebilir.

## 6. Hata durumu testleri (servisin "yanlış" bir şey yapmadığını gösterir)
| Kimlik | Durum | Beklenen |
|---|---|---|
| T-30 | PSI dosyası açılamıyor (eski çekirdek simülasyonu: tetikleyici yazımı başarısız) | Servis açık bir hata mesajıyla çıkar, sistemi değiştirmeden; systemd birimi `failed` |
| T-31 | Zram kurulamıyor (`modprobe` başarısız) | Uyarı yazılır; politika disk takasıyla çalışmaya devam eder |
| T-32 | Ajan hiç çalışmıyor | Servis çalışır, kimseyi dondurmaz (odak bilgisi yok = herkes "yeni" sayılır); günlükte uyarı |
| T-33 | Geçersiz JSON veya 64 KB'tan büyük mesaj | Bağlantı kapatılır, servis çalışmaya devam eder |
| T-34 | Dondurulan uygulama dondurma sırasında kapanır | Hata günlüğe düşer, tablo temizlenir, çökme yok |

## 7. Sonuç kayıt şablonu
`results/test_sonuclari.md` dosyasında tutulur ve rapordaki "Test ve değerlendirme" bölümünün kaynağıdır.

```markdown
| Test | Tarih | Yapan | Ortam (commit) | Sonuç | Not / kanıt |
|---|---|---|---|---|---|
| T-01 | 2026-12-01 | Melih | a1b2c3d | GEÇTİ | swapon çıktısı: results/kanit/T-01.txt |
| T-07b | 2026-12-01 | Furkan | a1b2c3d | KALDI → düzeltildi (e4f5a6b) → GEÇTİ | ExecStopPost çalışmıyordu: B-1x |
```
Kalan her test, düzeltilene kadar [Riskler ve bulgular](08_riskler_ve_bulgular.md)'a bir bulgu olarak yazılır.
