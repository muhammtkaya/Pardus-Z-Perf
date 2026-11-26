# 08 · Riskler ve Bulgular

Bu doküman yaşayan bir kayıttır: her gün sonunda güncellenir. Bulgular, teknik rapordaki "Gerçekleştirim: karşılaşılan problemler" ve "Sonuç ve kısıtlar" bölümlerinin doğrudan kaynağıdır.

## 1. Risk kaydı
**Olasılık / Etki:** Y = yüksek, O = orta, D = düşük.

| Kimlik | Risk | Olasılık | Etki | Önlem | Tetikleyici olursa | Sahibi |
|---|---|---|---|---|---|---|
| R-01 | **Takvim:** 11 günde kod, ölçüm, rapor ve video yetişmez | Y | Y | Günlük plan ([İş planı](10_is_plani.md)), 2 Aralık'ta özellik dondurma, kapsam daraltma sırası ([Kapsam](03_kapsam.md)) | Her akşam checklist'te geride kalınan iş varsa ertesi sabah kapsam daraltılır | Ortak |
| R-02 | Geçici scope ile süreç sahiplenme (TK-02) systemd tarafından reddedilir veya uygulamaları bozar | O | Y | 26 Kasım'da 06-7.1 deneyi | Önerideki yönteme (cgroupfs, `zperf.slice`) dönülür; `units.c` arayüzü aynı kalır | Furkan |
| R-03 | Oturum grubundan çıkarılan uygulamalar logind'in oturumu dışında kalır; yetki pencereleri, dosya pencereleri vb. bozulur | O | O | T-15 testi; GNOME'da uygulamaların zaten oturum grubunun dışında çalıştığı biliniyor (B-05) | Sorunlu uygulamalar dışlama listesine eklenir ve raporda kısıt olarak yazılır | Furkan |
| R-04 | Dondurulan uygulamada yan etki: ses kesilir, ağ bağlantısı zaman aşımına uğrar, pencere "yanıt vermiyor" der | O | O | Ses çalanlar dondurulmaz (TK-08); "asla dondurma" listesi; boşta kalma süresi | Gözlenen yan etkiler bulgu olarak yazılır; gerekirse ilgili uygulama varsayılan dışlama listesine eklenir | Melih |
| R-05 | Zayıf CPU'da sıkıştırma maliyeti kazancı yer | O | O | lz4 varsayılan; CPU sınırlı ölçüm; M5 | Sonuç raporlanır; "Akıllı Tahta" profilinde `reclaim_adim` küçültülür | Melih |
| R-06 | Bellek doluyken zram'i kapatmak (`swapoff`) sistemi kilitler | D | Y | Servis zram'i hiçbir zaman çalışırken kapatmaz; yalnızca paket kaldırılırken ve `MemAvailable` yeterliyse kapatılır | Kaldırma betiği yeterli bellek yoksa takası açık bırakır ve uyarı verir | Melih |
| R-07 | Ölçümler sanal makinede beklenen farkı göstermez (C ile D arası fark küçük) | O | O | Yük, B'de thrashing başlayacak şekilde ayarlanır; profil eşikleri bir kez ayarlanabilir | Sonuç olduğu gibi raporlanır, nedenleri tartışılır; proje yine geçerlidir (öneri hedefleri "başarı ölçütü", "geçme şartı" değil) | Melih |
| R-08 | `libwnck` bazı pencereler için PID vermez (`get_pid() == 0`) | O | D | 06-7.3 deneyi | Bu uygulamalar yönetilmez ve kısıt olarak listelenir | Melih |
| R-09 | Root servisteki bir hata kullanıcının uygulamasını donuk bırakır veya sistemi kararsızlaştırır | D | Y | `ExecStopPost` ile thaw-all, açılışta thaw-all, `OOMScoreAdjust`, ASan ile test, T-07 ve T-30–T-34 | Hata düzeltilene kadar varsayılan profil "Ofis" (dondurmasız) yapılır | Furkan |
| R-10 | Bir üye hastalanır veya başka bir sınava çalışmak zorunda kalır | O | Y | Kod incelemesiyle her üye diğerinin kodunu tanır; dokümanlar ortak | İş planındaki görevler yeniden dağıtılır, kapsam daraltılır | Ortak |
| R-11 | Video 8–10 dakikayı tutturamaz veya kayıt bozulur | O | O | Senaryo yazılı ([Demo ve sunum](12_demo_video_ve_sunum.md)); 5 Aralık'ta kayıt, 6 Aralık yedek | Bölümler ayrı ayrı çekilip birleştirilir | Ortak |
| R-12 | Teslim zip'inde eksik veya fazla dosya var; README başka makinede çalışmıyor | O | Y | Checklist bölüm 4; T-14 temiz kurulum testi | — | Melih |
| R-13 | Pardus deposunda beklenen paket yok (`gir1.2-notify-0.7`, `socat`, `libjson-c5` adı) | O | D | İlk gün `apt-cache policy` ile kontrol | Bildirim için `notify-send`; `socat` yerine Python ile test istemcisi; paket adı düzeltilir | Melih |

## 2. Bulgular
**Durum değerleri:**
- **Doğrulandı:** Kanıtı var.
- **Çıkarım:** Mantıkla varılan, henüz test edilmemiş.
- **Doldurulacak:** Deney yapılmadı.

| Kimlik | Bulgu | Kaynak / kanıt | Durum | Projeye etkisi |
|---|---|---|---|---|
| B-01 | Pardus 25'in varsayılan kurulumunda zram kapalı; 2,1 GB disk takas bölümü var, öncelik -2; `vm.swappiness=60`, `page-cluster=3` | `on_inceleme_pardus_20261017_2255.txt` (GNOME), `..._20261018_0023.txt` (XFCE) | Doğrulandı | Problem tanımının temeli; B yapılandırması varsayılan durumdur |
| B-02 | PSI derlenmiş ve varsayılan açık (`CONFIG_PSI=y`, `PSI_DEFAULT_DISABLED` yok); `/proc/pressure/{cpu,io,memory}` mevcut | Aynı dosyalar, bölüm 4 | Doğrulandı | Ek açılış parametresi gerekmez |
| B-03 | cgroup v2 (`cgroup2fs`); `memory.reclaim`, `cgroup.freeze` dosyaları var. Kökte etkin denetleyiciler: `cpu memory pids`; `io` etkin değil | Aynı dosyalar, bölüm 5 | Doğrulandı | `io.max` ile disk yavaşlatma için ek ayar gerekir; bu yüzden VirtualBox bant sınırı kullanılıyor |
| B-04 | Zram: varsayılan lz4; lz4hc, zstd, deflate mevcut; `ZRAM_WRITEBACK=y`; `ZRAM_MULTI_COMP` kapalı | Aynı dosyalar, bölüm 6 ve 10 | Doğrulandı | Writeback mümkün (ek işlev); yeniden sıkıştırma mümkün değil |
| B-05 | XFCE: X11 oturumu; tüm uygulamalar tek `session-2.scope` içinde. GNOME: Wayland; uygulamalar `user@1000.service/app.slice/app-gnome-*.scope` içinde | Aynı dosyalar, bölüm 2 ve 5 | Doğrulandı | XFCE'de uygulamaları ayırmak gerekli (TK-02); GNOME düzeni TK-02 için örnek oluşturur |
| B-06 | `policykit-1` paketi yok, `polkitd` ve `pkexec` kurulu; `gir1.2-wnck-3.0` depoda var ama kurulu değil; `stress-ng` depoda | XFCE dosyası, bölüm 7 | Doğrulandı | Paket bağımlılıkları buna göre yazıldı |
| B-07 | cgroup v2'de taşınan sürecin önceden ayırdığı bellek eski grupta sayılmaya devam eder | Çekirdek belgeleri, cgroup-v2 "Memory Ownership" | Belgeye dayalı; T-03/T-04'te gözlenecek | TK-03: uygulamalar ilk görüldükleri anda gruplanır |
| B-08 | Gruba alınan uygulamanın logind tarafından görülmesi ve kaydet/aç pencerelerinin çalışması | 06-7.1 deneyi, 5. ve 6. adımlar | **Doldurulacak** | TK-02'nin kabulü ve R-03 |
| B-09 | Sanal makineler `VERSION_ID="25.2"`; önerinin son halinde "Pardus 25.0" yazıyor | İki ön inceleme dosyası, bölüm 1 | Doğrulandı | Teslim belgelerinde "Pardus 25 (25.2 ile test edildi)" yazılır |
| B-10 | VirtualBox sanal diski `ROTA=1` (dönen disk) gösteriyor ama gerçekte ana bilgisayarın SSD'sinde duruyor | XFCE dosyası, bölüm 9 | Doğrulandı | Disk bant genişliği sınırı şart; raporda geçerlilik tehdidi olarak yazılır |
| B-11 | Kullanıcının systemd yöneticisi, süreçleri sistemin oluşturduğu `session-N.scope`'tan çıkaramaz; bu yüzden sahiplenmeyi root servis yapmalıdır | cgroup v2 devretme kuralları | Çıkarım; 06-7.1 deneyinde doğrulanacak | TK-02'nin servis tarafında uygulanma gerekçesi |
| B-12 | `pactl -f json` çıktısı ve `application.process.id` alanı PipeWire 1.4 altında | 06-8 | **Doldurulacak** | TK-08 |
| B-13 | `libwnck` ile PID alınamayan uygulamalar | 06-7.3 deneyi | **Doldurulacak** | R-08 |

## 3. Yeni bulgu ekleme şablonu
```markdown
| B-14 | (Ne gözlendi, tek cümle) | (komut çıktısı dosyası / commit / test kimliği) | Doğrulandı | (hangi karar, gereksinim veya rapor bölümü etkileniyor) |
```
Her önemli bulgu için kanıt dosyası `results/kanit/B-xx.txt` olarak saklanır. Raporda "karşılaşılan problem" olarak anlatılacak olanları (örn. B-07, B-08) işaretleyin; sözlü sunumda bunlar "neler öğrendiniz?" sorusunun en iyi cevabıdır.
