# 03 · Kapsam

## Kapsam içi (teslimde çalışır durumda olacak)
| Alan | İçerik | Gereksinimler |
|---|---|---|
| Servis | zram kurulumu, PSI olay döngüsü, uygulamaları geçici scope'lara alma, kademe 1 ve 2, odakla çözme, güvenli kapanış, günlük kaydı | G-01 – G-15 |
| Ajan | Otomatik başlama, pencere ve odak bildirimi, ses algılama, bildirimler, yeniden bağlanma | G-20 – G-25 |
| Arayüz | Durum ekranı, profil seçimi, açma/kapatma, "asla dondurma" | G-30 – G-33 |
| Profiller | Dengeli, Akıllı Tahta, Ofis | G-13 |
| Kurulum | `make install` ve `.deb` paketi, systemd birimi, autostart, polkit eylemi, kaldırma | G-53, G-55 |
| Ölçüm | Etkileşim yoklayıcısı, veri toplayıcı, yük betiği, analiz; A–D karşılaştırması | G-40 – G-43 |
| Belgeler | README, teknik rapor, 8–10 dakikalık video, VIDEO_LINK.txt | Aşama 2 bölüm 2 |

## Kapsam dışı
| Konu | Neden | Raporda nasıl geçecek |
|---|---|---|
| Sekme bazında yönetim | Tarayıcı içine erişim gerektirir; servis tarayıcıyı tek uygulama olarak görür. | Kısıt |
| GNOME / Wayland desteği | libwnck Wayland'da çalışmaz; GNOME için ayrı bir odak kaynağı gerekir. | Gelecek çalışma (öneride "ek") |
| Kademe 3 (kullanıcıya sorarak kapatma) | Önerideki ek işlev; süre yetmiyor. | Gelecek çalışma |
| zram writeback | Önerideki ek işlev; çekirdek destekliyor (B-04) ama süre yetmiyor. | Gelecek çalışma |
| Yeniden sıkıştırma | Pardus 25 çekirdeğinde `CONFIG_ZRAM_MULTI_COMP` kapalı (B-04). | Kısıt |
| Pardus 23 ve ETAP üzerinde test | Süre ve donanım erişimi yok. | Kısıt |
| Gerçek eski donanımda test | Erişim olursa yapılır, olmazsa sanal makine sonuçlarıyla yetinilir. | Kısıt veya bulgu |
| Canlı grafikler, sistem tepsisi | Önerideki ek işlev. | Gelecek çalışma |

## Aşama 1'den yapılan değişiklikler
Aşama 2 dokümanı, temel amaç ve özgün katkı korunduğu sürece kapsamın güncellenmesine izin veriyor. Aşağıdaki değişikliklerin hepsi teknik raporun "Gerçekleştirim" bölümünde gerekçesiyle yazılmalı.

| # | Önerideki hali | Yeni hali | Gerekçe | Temel amaca etkisi |
|---|---|---|---|---|
| D-1 | 9 haftalık takvim, 18 Aralık'a kadar | 11 günlük takvim, 7 Aralık teslim | Nihai teslim tarihi 7 Aralık olarak ilan edildi. | Yok |
| D-2 | Servis süreçleri kendi oluşturduğu `/sys/fs/cgroup/zperf.slice` ağacına doğrudan taşır | Servis, systemd'den süreçler için geçici scope ister (`StartTransientUnit`, `PIDs` özelliği); dondurma `FreezeUnit` ile yapılır | systemd'nin yönettiği ağaçla çakışmayı önler; GNOME'un kullandığı yöntemle aynıdır. **TK-02, 1. gün deneyle doğrulanacak; olmazsa önerideki yönteme dönülür.** | Yok; aynı cgroup mekanizması |
| D-3 | Uygulama dondurulmadan önce gruba alınır | Uygulama ilk görüldüğü anda gruba alınır | cgroup v2'de süreç taşınınca daha önce ayırdığı bellek eski grupta sayılmaya devam eder (B-07). Geç taşınan uygulamada `memory.reclaim` neredeyse işe yaramaz. | Olumlu; kademe 1 ve 2'nin çalışması için gerekli |
| D-4 | Ses algılama servisin işi gibi duruyordu | Ses çalan uygulamaları ajan bulur (`pactl`), servise bildirir | Ses sunucusu kullanıcı oturumunda çalışır; root servisin oraya erişmesi gereksiz karmaşıklık. | Yok |
| D-5 | Her yapılandırma en az 5 kez ölçülür | En az 3 kez; süre kalırsa 5 | Takvim. | Ölçüm güvenilirliği biraz düşer; raporda belirtilir. |
| D-6 | Hedef "Pardus 25.0" | "Pardus 25 (25.2 üzerinde test edildi)" | Sanal makineler 25.2 (B-09). | Yok |
| D-7 | Pardus 23 ve ETAP uyumluluk testi | Kapsam dışı | Takvim ve erişim. | Yok; zaten "erişim olursa" diye yazılmıştı |

## Kapsam daraltma sırası (gecikme olursa)
Önerideki sıra korunmuştur. Bir adım ancak bir öncekinden vazgeçildikten sonra uygulanır:

1. Ek işlevlerin hiçbiri yapılmaz (zaten planda yok).
2. Ses algılama yerine yalnızca "asla dondurma" listesi kullanılır (G-23 düşer).
3. Profil sayısı ikiye iner: Dengeli ve Akıllı Tahta.
4. `.deb` yerine yalnızca `make install` ve `install.sh` (G-53 korunur).
5. Arayüz yalnızca durum gösterimine iner; profil ve açma/kapatma komut satırı aracından yapılır (`zperfctl`).
6. Ölçüm, A–D yerine yalnızca C ve D karşılaştırmasına iner.

**Asla kesilmeyecekler:**
- Servis: kademe 1 ve 2, odakla çözme, güvenli kapanış (G-02 – G-09).
- C ve D karşılaştırması.
- README, teknik rapor, video.
