# 01 · Proje Özeti

## Tek cümlede
Z-Performans, düşük RAM'li Pardus bilgisayarlarında bellek dolduğunda sistemin kilitlenmesini önlemek için, kullanıcının o an kullanmadığı uygulamaların belleğini sıkıştırılmış RAM'e (zram) iten ve gerekirse bu uygulamaları donduran, kullanıcı geri döndüğünde onları çözen bir sistem servisidir.

## Problem
- **Hedef cihazlar:** Okullardaki etkileşimli tahtalar ve kamu kurumlarındaki eski bilgisayarlar. Bunlarda RAM azdır ve disk genellikle yavaş bir HDD'dir.
- **Ne oluyor:** Tarayıcıda çok sekme ve birkaç uygulama açılınca bellek dolar. Çekirdek sayfaları yavaş diske taşımaya başlar ve sistem dakikalarca tepkisiz kalır (thrashing). Çekirdeğin OOM killer'ı genellikle çok geç devreye girer ve kullanıcıya sormadan bir uygulamayı kapatır.
- **Kullanıcının bugünkü çözümü:** Cihazı güç düğmesinden kapatıp açmak. Kaydedilmemiş iş kaybolur.
- **Pardus'taki durum (ön incelemede doğrulandı):** Pardus 25'in varsayılan kurulumunda zram kapalıdır. Sistem diskteki 2,1 GB'lık takas bölümünü kullanır (`vm.swappiness=60`).

## Hedef kullanıcılar
- **Birincil:** Teknik bilgisi olmayan öğretmenler, öğrenciler ve memurlar. Hiçbir ayar yapmadan faydalanmalılar.
- **İkincil:** Okul ve kurum BT sorumluları. Kurulumu yapar, profili seçerler.

## Çözüm
Linux çekirdeğinin üç mekanizması tek bir politika altında birleştirilir:

| Mekanizma | Ne işe yarar |
|---|---|
| **PSI** (`/proc/pressure/memory`) | Bellek baskısını "boş bellek" olarak değil, "işlerin bellek yüzünden ne kadar beklediği" olarak ölçer. Eşik aşılınca servisi uyandırır. |
| **zram** | Takasa giden sayfaları diske değil, sıkıştırılmış olarak RAM'e yazar. |
| **cgroup v2** | Her uygulamayı ayrı bir gruba koyar. `memory.reclaim` ile o grubun soğuk sayfalarını zram'e iter, `freeze` ile grubu dondurur. |

Servis bellek baskısına göre kademeli tepki verir:

1. **Kademe 1, proaktif sıkıştırma:** Odakta olmayan uygulamaların soğuk sayfaları zram'e itilir.
2. **Kademe 2, dondur ve sıkıştır:** Baskı sürerse en uzun süredir kullanılmayan uygulama dondurulur ve belleği sıkıştırılır.
3. **Geri dönüş:** Kullanıcı o uygulamaya geçtiği anda uygulama çözülür. Hiçbir uygulama öldürülmez.

## Bileşenler
| Bileşen | Dil | Görev | Sorumlu |
|---|---|---|---|
| `zperfd` | C | Root olarak çalışan systemd servisi: PSI izleme, zram, cgroup ve systemd işlemleri, politika | Furkan |
| `zperf-agent` | Python | Kullanıcı oturumunda çalışır: odaktaki pencere, yeni pencereler, ses çalan uygulamalar, bildirimler | Melih |
| `zperf-gui` | Python + GTK 3 | Durum gösterimi, profil seçimi, açma/kapatma, "asla dondurma" listesi | Melih |
| Ölçüm araçları | C + Python | Etkileşim yoklayıcısı (Furkan), yük betiği ve veri toplayıcı (Melih) | İkisi |

## İşletim sistemleri dersiyle bağı
- **Sanal bellek, sayfalama, takas:** zram bir takas aygıtıdır; `memory.reclaim` çekirdeğin sayfa geri kazanım yolunu tetikler.
- **Sayfa değiştirme politikaları:** Çekirdek hangi sayfanın soğuk olduğuna LRU listeleriyle karar verir. Projenin politikası bunun üstünde, uygulama düzeyinde bir ikinci karar katmanıdır.
- **Süreç yönetimi:** Süreç ağaçları, cgroup ile gruplama, dondurma ve çözme, sinyaller.
- **Çekirdek–kullanıcı alanı arayüzleri:** `/proc`, `/sys`, `poll()`, `swapon(2)`, Unix soketleri, D-Bus.

## Başarı nasıl gösterilecek?
Aynı yük altında dört yapılandırma karşılaştırılır:
- **A:** Takas yok.
- **B:** Disk takası.
- **C:** zram-tools varsayılanı.
- **D:** Z-Performans.

Öneride belirlenen hedefler D'nin C'ye göre şunları sağlaması:
- Ön plandaki takılma süresinde (M1) **en az %30 azalma**.
- Tepkisizliğe kadar açılabilen sekme sayısında (M3) **en az %20 artış**.
- OOM killer'ın hiçbir süreci kapatmaması (**M4 = 0**).
- Donmuş bir uygulamanın odaklanınca **3 saniyenin altında** kullanılabilir hale gelmesi (M6).

Ayrıntılar [Test planı](07_test_plani.md)'nda.

## Mevcut durum (26 Kasım 2026)
- **Tamam:**
  - Aşama 1 önerisi teslim edildi.
  - Pardus 25 GNOME ve XFCE sanal makinelerinde ön inceleme yapıldı.
  - Çekirdek özellikleri doğrulandı (bkz. [Bulgular](08_riskler_ve_bulgular.md)).
- **Başlanacak:** Kodlama, ölçümler, README, teknik rapor, video.
