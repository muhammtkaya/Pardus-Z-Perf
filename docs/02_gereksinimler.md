# 02 · Gereksinimler

## Öncelik sınıfları
| Sınıf | Anlamı |
|---|---|
| **Z (Zorunlu)** | Önerideki asgari üründe var; teslimde çalışmıyorsa projenin temel işlevi eksik sayılır. |
| **Ö (Önemli)** | Kalite, güvenlik veya ölçüm için gerekli; zaman daralırsa basitleştirilebilir ama çıkarılmamalı. |
| **E (Ek)** | Önerideki "zaman kalırsa" listesi. Yapılmazsa raporda "kısıt" olarak yazılır. |

Her gereksinimin **kabul ölçütü**, onu doğrulayan testin kimliğiyle verilmiştir (bkz. [Test planı](07_test_plani.md)). "Öneri" sütunu, gereksinimin Aşama 1 önerisindeki karşılığını gösterir.

## İşlevsel gereksinimler

### Servis (zperfd)
| Kimlik | Gereksinim | Öncelik | Kabul ölçütü | Öneri |
|---|---|---|---|---|
| G-01 | Servis, systemd tarafından açılışta root olarak başlatılır; çökerse systemd yeniden başlatır. | Z | T-01, T-07 | Bileşenler |
| G-02 | Servis, açılışta zram takası kurar: lz4, RAM'in %50'si, öncelik 100. Zaten bir zram takası varsa onu kullanır, yenisini kurmaz. | Z | T-01 | Zorunlu 1 |
| G-03 | Servis, `/proc/pressure/memory` üzerine PSI tetikleyicileri kurar ve `poll()` ile bekler; sürekli yoklama (busy loop) yapmaz. | Z | T-02 | Zorunlu 1 |
| G-04 | Ajan tarafından bildirilen her yeni uygulama penceresinin süreç ağacı, ilk bildirimden sonra en geç 2 saniye içinde kendi cgroup'una (systemd geçici scope) alınır. | Z | T-03 | Gruplama, TK-02 |
| G-05 | **Kademe 1:** PSI `some` eşiği aşılınca, odakta olmayan ve hariç tutulmamış uygulamaların cgroup'larına adım adım `memory.reclaim` yazılır. | Z | T-04 | Zorunlu 2 |
| G-06 | **Kademe 2:** Daha yüksek eşik aşılınca, odakta olmayan, profilin boşta kalma süresini doldurmuş, hariç tutulmamış ve ses çalmayan uygulamalar arasından en çok bellek kullanan dondurulur; ardından belleği zram'e itilir. | Z | T-05 | Zorunlu 2 |
| G-07 | Donmuş bir uygulama odak aldığında 1 saniye içinde çözülür. | Z | T-06 | Zorunlu 2 |
| G-08 | Servis kapanırken (SIGTERM), çöktüğünde veya yeniden başladığında hiçbir uygulama donuk kalmaz. | Z | T-07 | Zorunlu 3 |
| G-09 | "Asla dondurma" listesindeki uygulamalar ve sistem bileşenleri (Xorg, xfwm4, panel, masaüstü, PipeWire, ajan, arayüz) hiçbir koşulda dondurulmaz. | Z | T-08 | Zorunlu 2 |
| G-10 | Baskı düşünce (histerezis ile) servis kademe 0'a döner; donmuş uygulamaları kendiliğinden çözmez, onları odak alınca çözer. | Ö | T-04, T-05 | Kademeler |
| G-11 | Servis hiçbir süreci öldürmez. | Z | Kod incelemesi, T-05 | Net katkı |
| G-12 | Servis, profili yapılandırma dosyasından okur; çalışırken IPC ile değiştirilebilir. | Z | T-09 | Zorunlu 5 |
| G-13 | En az iki profil: "Dengeli" ve "Akıllı Tahta". "Ofis" üçüncü profil. | Z (2), Ö (3.) | T-09 | Zorunlu 5 |
| G-14 | Servis, durum bilgisini IPC ile verir: kademe, PSI değerleri, zram istatistikleri, uygulama listesi. | Z | T-10 | Zorunlu 5 |
| G-15 | Servis, her kademe değişimini, dondurmayı, çözmeyi ve geri kazanılan bellek miktarını zaman damgasıyla günlüğe (journal) yazar. | Ö | T-04, T-05 | Ölçümler |

### Ajan (zperf-agent)
| Kimlik | Gereksinim | Öncelik | Kabul ölçütü | Öneri |
|---|---|---|---|---|
| G-20 | Ajan, XFCE oturumu açılınca otomatik başlar (`/etc/xdg/autostart`). | Z | T-11 | Zorunlu 4 |
| G-21 | Ajan, açılışta mevcut tüm pencereleri, sonra her yeni pencereyi PID ve uygulama adıyla servise bildirir. | Z | T-03 | Zorunlu 4, TK-03 |
| G-22 | Ajan, her odak değişimini 200 ms içinde servise bildirir. | Z | T-06 | Zorunlu 4 |
| G-23 | Ajan, ses çalan uygulamaların PID'lerini en fazla 5 saniyede bir servise bildirir. | Ö | T-08 | Kademeler |
| G-24 | Ajan, dondurma ve çözme olaylarında masaüstü bildirimi gösterir (kapatılabilir). | Ö | T-11 | Bileşenler |
| G-25 | Servis bağlantısı koparsa ajan 2 saniyede bir yeniden bağlanmayı dener ve bağlanınca pencere listesini yeniden gönderir. | Ö | T-07 | Sağlamlık |

### Arayüz (zperf-gui)
| Kimlik | Gereksinim | Öncelik | Kabul ölçütü | Öneri |
|---|---|---|---|---|
| G-30 | Saniyede bir güncellenen durum ekranı: kademe, PSI `some` ve `full` avg10, RAM kullanımı, zram sıkıştırma oranı, uygulama listesi (bellek, takas, donuk mu). | Z | T-10 | Zorunlu 5 |
| G-31 | Profil seçimi. | Z | T-09 | Zorunlu 5 |
| G-32 | Politikayı açma/kapatma. Kapatınca tüm uygulamalar çözülür; zram açık kalır. | Z | T-09 | Zorunlu 5 |
| G-33 | Uygulama başına "asla dondurma" işaretlemesi. | Ö | T-08 | Bileşenler |
| G-34 | Canlı bellek grafikleri ve sistem tepsisi simgesi. | E | — | Ek işlevler |

### Ölçüm araçları
| Kimlik | Gereksinim | Öncelik | Kabul ölçütü |
|---|---|---|---|
| G-40 | Etkileşim yoklayıcısı: her 100 ms'de uyanıp kendi belleğine dokunur; 200 ms'yi aşan gecikmeleri CSV'ye yazar (M1, M3). | Z | T-20 |
| G-41 | Veri toplayıcı: saniyede bir PSI, `/proc/vmstat` takas sayaçları, zram `mm_stat`, CPU kullanımı ve servis olaylarını CSV'ye yazar (M2, M5, M6). | Z | T-20 |
| G-42 | Yük betiği: aynı yükü her çalıştırmada aynı sırayla üretir. | Z | T-20 |
| G-43 | Analiz betiği: CSV'lerden M1–M6 tablolarını ve grafikleri üretir. | Z | T-21 |

## İşlevsel olmayan gereksinimler
| Kimlik | Gereksinim | Öncelik | Doğrulama |
|---|---|---|---|
| G-50 | **Hedef platform:** Pardus 25 XFCE (25.2 ile test), Linux 6.12, X11 oturumu. | Z | Tüm testler bu ortamda |
| G-51 | **Kaynak kullanımı:** Servis boşta %1'in altında CPU ve 10 MB'ın altında RSS; ajan ve arayüz 60 MB'ın altında. | Ö | T-12 |
| G-52 | **Güvenlik:** Servis yalnızca, isteği gönderen kullanıcıya (SO_PEERCRED ile doğrulanan UID) ait ve UID'i 1000 veya üstü olan süreçleri yönetir. Ayar değiştiren komutlar polkit yetkisi ister (`allow_active=yes`). | Z | T-13 |
| G-53 | **Kurulum:** Projeyi daha önce görmemiş biri, temiz bir Pardus 25 XFCE'de README'yi izleyerek 15 dakikada kurup çalıştırabilir. | Z | T-14 |
| G-54 | **Lisans:** Grubun kodu GPL-3.0; üçüncü taraf bileşenler README'de lisanslarıyla listelenir. | Z | Checklist |
| G-55 | **Geri alınabilirlik:** Paket kaldırılınca zram takası kapatılır, systemd birimleri ve yapılandırma silinir, `vm.swappiness` eski değerine döner. | Ö | T-14 |

## İzlenebilirlik: önerideki zorunlu işlevler
| Önerideki zorunlu işlev | Karşılayan gereksinimler |
|---|---|
| 1. zperfd: zram kurulumu ve PSI olay döngüsü | G-01, G-02, G-03 |
| 2. Kademe 1 ve 2, "asla dondurma" listesi | G-04, G-05, G-06, G-07, G-09 |
| 3. Servis kapanınca veya çökünce tüm uygulamaların çözülmesi | G-08 |
| 4. zperf-agent ile odak bilgisinin iletilmesi | G-20, G-21, G-22 |
| 5. zperf-gui: durum, açma/kapatma, en az iki profil | G-12, G-13, G-14, G-30, G-31, G-32 |
| 6. .deb paketi, kurulum belgesi, karşılaştırmalı ölçüm raporu | G-53, G-40–G-43 |
