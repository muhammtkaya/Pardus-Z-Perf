# 12 · Demo, Video ve Sözlü Sunum

## 1. Demo videosu (8–10 dakika)
**Şartlar (Aşama 2, 2.4):**
- Yazılımın çalışması gösterilmeli; yalnızca slayttan oluşmamalı.
- Gerçek bir kullanım senaryosu üzerinden anlatılmalı.
- Temel işlevler ve katkı gösterilmeli.

**Hazırlık:**
- **Kayıt:** OBS Studio ile ana bilgisayarda. Sanal makine penceresi tam ekran, 1920×1080.
- **Ses:** Kulaklık mikrofonu; iki üye kendi bölümünü anlatır.
- **Sanal makine:** Ölçüm ayarlarıyla (2 GB RAM). Demodan önce bir prova yapılır; yük, sistem kilitlenmeden 2–3 dakikada dondurmaya ulaşacak şekilde ayarlanır.
- **Yan yana demo:** İki sanal makine aynı anda çalıştırılacaksa (C solda, D sağda), ana bilgisayarın belleği yeterli olmalı. Yetmezse C ve D art arda çekilir ve aynı süre damgasıyla yan yana kurgulanır.

**Senaryo:**
| Süre | Bölüm | Ekranda ne var | Anlatan | Söylenecek ana fikir |
|---|---|---|---|---|
| 0:00–0:45 | Problem | Gerçek bir yavaşlık: B yapılandırmasında (disk takası) sekmeler açılırken farenin donması, yoklayıcının takılma sayacı | M | Okul ve kurum bilgisayarlarında bellek dolunca sistem dakikalarca kilitleniyor ve kullanıcı cihazı kapatıp açıyor. Pardus'ta zram varsayılan olarak kapalı. |
| 0:45–1:45 | Çözüm fikri ve katkı | Tek bir mimari şema (Şekil 1) ve mevcut araçlar tablosu | F | PSI, zram, cgroup; diğer araçlar ya sadece zram açar ya süreç öldürür; biz önce sıkıştırıp donduruyoruz ve kullanıcı döndüğünde çözüyoruz. |
| 1:45–2:45 | Kurulum | `sudo apt install ./zperf_0.1.0_amd64.deb`; `systemctl status zperfd`; `swapon --show`; arayüzün açılması | M | Pakette ne var; zram'i servis kuruyor; ajan oturumla başlıyor. |
| 2:45–3:45 | Erken sahiplenme | Mousepad ve Firefox açılır; `systemd-cgls` ile uygulamaların kendi scope'larına taşındığı gösterilir | F | XFCE'de her şey tek grupta; neden ayırıyoruz; neden ilk anda ayırıyoruz (bellek eski grupta sayılmaya devam eder). |
| 3:45–6:15 | **Ana senaryo** | Kabul senaryosu (07-4): ön planda video, arka planda LibreOffice (kaydedilmemiş metin) ve PDF, sekmeler açılıyor. Arayüzde kademe 0 → 1 → 2, uygulamaların takas miktarı artıyor, LibreOffice "donuk" oluyor, bildirim geliyor. Video akmaya devam ediyor. | M anlatır, F günlüğü (`journalctl -fu zperfd`) yan pencerede gösterir | Kademe 1 ne yaptı, kademe 2 neden LibreOffice'i seçti (odakta değil, en uzun süre boşta, en büyük). |
| 6:15–7:00 | Geri dönüş | Alt+Tab ile LibreOffice'e geçiş; günlükte `thawed thaw_ms=…`; metin kaybolmamış | F | Hiçbir uygulama öldürülmedi; çözülme süresi. |
| 7:00–7:45 | Güvenlik ağı | Bir uygulama donukken `sudo kill -9 $(pidof zperfd)`: uygulama çözülüyor, servis yeniden başlıyor | F | Root servis çökerse bile kimse donuk kalmıyor (`ExecStopPost`). |
| 7:45–9:00 | Ölçüm sonuçları | Şekil 4 ve 5, M1–M6 tablosu | M | C ile D farkı; hedefler tuttu mu; maliyet (CPU). Tutmayan hedef varsa nedeni. |
| 9:00–9:40 | Kısıtlar ve kapanış | Kısıtlar listesi | F + M | Sanal makine, GNOME/Wayland yok, sekme bazında değil; gelecek çalışmalar. |

**Yayınlama:**
- **Yükleme:** YouTube'a "liste dışı" (unlisted) olarak yüklenir. Bağlantı, çıkış yapılmış bir tarayıcıda denenir.
- **`VIDEO_LINK.txt` içeriği:**
  ```
  Pardus Z-Performans: Demo Videosu (9 dk 40 sn)
  https://youtu.be/...
  ```

## 2. Sözlü sunum akışı (önerilen 10–12 dakika + sorular)
1. **Problem ve amaç** (M, 1 dk): Kullanım senaryosu tek cümleyle.
2. **Mimari** (F, 2 dk): Şekil 1; servis, ajan, arayüz; çekirdek arayüzleri.
3. **Katkı** (F, 1 dk): 04'teki "kısa cevap".
4. **Canlı demo** (M sürer, F anlatır, 4 dk): Videodaki 3:45–7:00 bölümünün canlı hali.
   - Canlı demo bozulursa hemen videonun ilgili bölümüne geçilir; video bilgisayarda açık bekler.
5. **Sonuçlar** (M, 2 dk): Şekil 5 ve tablo.
6. **Karşılaşılan problemler ve öğrendiklerimiz** (her üye bir tane, 1 dk): F → B-07 (bellek sahipliği), M → B-08 veya ölçümde karşılaşılan bir problem.
7. **Kısıtlar** (1 dk).

## 3. Olası sorular ve cevap hazırlığı
Aşama 2: "Her grup üyesi kendi katkısını ve projeye ilişkin teknik detayları açıklayabilmelidir."

**Kurallar:**
- Her soruyu **iki üye de** cevaplayabilmeli; birincil cevaplayan aşağıda yazılıdır.
- Cevaplar kısa tutulmalıdır; kısa cevaplar aşağıda verildi, ayrıntılı halini ilgili dokümandan çalışın.

### İşletim sistemi kavramları
| Soru | Kısa cevap | Birincil |
|---|---|---|
| PSI nedir, neden boş bellek miktarına bakmıyorsunuz? | PSI, işlerin bellek yüzünden ne kadar zaman kaybettiğini ölçer. Boş bellek az olabilir ama kimse beklemiyor olabilir (önbellek dolu). Kullanıcının hissettiği şey bekleme, o yüzden onu ölçüyoruz. | F |
| `some` ve `full` farkı? | `some`: en az bir iş bekliyor. `full`: çalışabilecek tüm işler aynı anda bekliyor, sistem hiç ilerlemiyor. | F |
| zram ile disk takası farkı? | Takasa giden sayfa diske değil, sıkıştırılıp RAM'e yazılır. Disk beklemek yerine CPU harcanır. HDD'de rastgele erişim milisaniyeler, zram mikrosaniyeler. | F |
| zram ile zswap farkı, neden zswap değil? | zswap, disk takasının önünde sıkıştırılmış bir önbellektir ve yine bir disk takasına ihtiyaç duyar. zram bağımsız bir takas aygıtıdır. Diski yavaş cihazlarda zram'in avantajı daha büyük, ayrıca zram'in istatistikleri ölçüm için doğrudan okunabiliyor. | F |
| `memory.reclaim` ne yapar, hangi sayfaları seçer? | Bir cgroup'a "şu kadar belleğini geri kazan" der. Çekirdek o grubun LRU listelerinden soğuk sayfaları seçer: anonim sayfalar takasa (zram'e) gider, temiz dosya önbelleği atılır. `EAGAIN` kısmi başarıdır. | F |
| Dondurma ile SIGSTOP farkı? | SIGSTOP süreçlere görünür, ebeveyn fark eder, başka bir sinyal devam ettirebilir ve süreç tek tek durdurulur. cgroup freezer tüm grubu birlikte ve süreçlere görünmeden dondurur. İkisi de belleği boşaltmaz; o yüzden ardından geri kazanım yapıyoruz. | F |
| Donmuş uygulamaya nasıl dönülüyor, pencere donukken odak nasıl değişiyor? | Odağı pencere yöneticisi (xfwm4) değiştirir; o ayrı bir süreç ve donmuyor. Ajan odak değişimini görür, servis grubu çözer. | M |
| Neden uygulamayı ilk görüldüğü anda gruba alıyorsunuz? | cgroup v2'de bellek, ayrıldığı grupta sayılmaya devam eder. Geç taşınan uygulamanın belleği eski grupta kalır ve `memory.reclaim` onu bulamaz (B-07). | F |
| Thrashing nedir? OSTEP'teki karşılığı? | Çalışma kümelerinin toplamı fiziksel belleği aşınca sistem zamanının çoğunu sayfa taşımaya harcar. OSTEP bölüm 22'de anlatılır; çözüm olarak süreçleri askıya alma (admission control) önerilir. Bizim dondurmamız bunun uygulamasıdır. | F |
| LRU ile sizin politikanızın farkı? | Çekirdeğin LRU'su sayfa düzeyinde ve kullanıcıdan habersiz. Biz uygulama düzeyinde, "kullanıcı bu uygulamaya bakıyor mu" bilgisini ekliyoruz. | F |

### Tasarım ve gerçekleştirim
| Soru | Kısa cevap | Birincil |
|---|---|---|
| Neden C ve Python? | TK-01: Servis root çalışıyor ve bellek baskısı anında bile tepki vermeli, bu yüzden küçük ve öngörülebilir olmalı. Arayüz ve ajanda GTK ile libwnck'e Python'dan erişmek hızlı. | F |
| Neden D-Bus değil de Unix soketi? | TK-06: Daha az kod, elle test edilebilir, SO_PEERCRED güvenilir kimlik veriyor. | M |
| Root servisinize kötü niyetli bir kullanıcı ne yapabilir? | Yalnızca kendi süreçlerini bildirebilir (UID denetimi). Ayar değiştirmek polkit ister. Uzunluk sınırı ve JSON doğrulaması var. Servis dosya yoluna istemci metni koymaz. | M |
| Servis çökerse ne olur? | `ExecStopPost` tüm grupları çözer, systemd servisi yeniden başlatır, servis açılışta yine çözer (T-07). | F |
| Neden geçici scope (systemd), neden doğrudan cgroupfs değil? | TK-02: systemd kendi ağacını yönetir; GNOME da bu yöntemi kullanıyor; grupların ömrünü systemd yönetiyor. (Deney sonucu B-08'den anlatılır.) | F |
| Profiller neye göre seçildi? | Başlangıç değerleri tahmindi; ölçümle bir kez ayarlandı (raporda gerekçe). Akıllı Tahta ön plandakini en agresif korur, Ofis hiç dondurmaz. | M |
| Ses çalan uygulamayı nasıl anlıyorsunuz? | Ajan `pactl` ile ses akışlarının sahibi süreçleri buluyor (TK-08). | M |
| Arayüz root değilken nasıl ayar değiştiriyor? | Servise istek gönderiyor; servis polkit'e "bu süreç yetkili mi" diye soruyor; etkin yerel oturuma izin veriliyor. | M |

### Ölçüm
| Soru | Kısa cevap | Birincil |
|---|---|---|
| Sonuçlarınıza ne kadar güvenilir? | 3 tekrar, karıştırılmış sıra, ortalama ± sd. Sınırlar: sanal makine, sentetik sekmeler, az tekrar (07-5.5). | M |
| Neden zram-tools ile karşılaştırdınız? | Pardus kullanıcısının bugün elle yapabileceği en iyi şey o. Fark politikadan geliyor, zram'in kendisinden değil. | M |
| M1'i nasıl ölçtünüz? | Her 100 ms'de uyanıp belleğine dokunan bir program; gecikmesi bellek baskısını doğrudan yansıtır. | F (yoklayıcıyı yazan) |
| Maliyeti nedir? | M5: CPU kullanımı ve sıkıştırma oranı (sonuç tablosundan). | M |

## 4. Prova
- **4 Aralık:** Videonun anlatım metni yüksek sesle bir kez okunur ve süre tutulur.
- **Sunumdan önceki gün:** Soru listesi karşılıklı sorulur. Her üye, diğerinin bileşeninden en az 5 soruya cevap verir.
