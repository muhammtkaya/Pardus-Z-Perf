# 11 · Teknik Rapor Planı

**Teslim dosyası:** `technical_report.pdf`

**Hedef uzunluk:** 14–18 sayfa (A4, 11 pt; şekiller ve tablolar dahil, ekler hariç).

**Yazım aracı:** Aşama 1 önerisi Word'de hazırlandı. Rapor da aynı stille Word'de yazılabilir; başlık stilleri kullanılır, içindekiler tablosu otomatik üretilir.

## 1. Bölümler, kaynaklar ve sorumlular
Başlıklar Aşama 2 dokümanındaki sırayla, aynı adlarla verilir. Değerlendiren kişi başlıkları kontrol edecektir.

| # | Başlık (Aşama 2 adıyla) | İçerik | Kaynak doküman | Yazan | Sayfa |
|---|---|---|---|---|---|
| 0 | Kapak ve içindekiler | Proje adı, ders, tarih, üyeler, öğrenci numaraları | — | M | 1 |
| 1 | Proje başlığı ve grup üyeleri | Ad, soyad, numara; her üyenin bir cümlelik rolü | 00, 10 | M | 0,5 |
| 2 | Problem ve amaç | Problem, hedef kullanıcılar, Pardus'taki durum (B-01), kullanım senaryosu, amaç | 01 | M | 1,5 |
| 3 | Projenin kapsamı ve katkısı | Geliştirilen işlevler (gereksinim tablosunun özeti), mevcut çözümler tablosu, net katkı, Aşama 1'den değişiklikler (D-1 – D-7) | 02, 03, 04 | F + M | 2 |
| 4 | Teknik yaklaşım | Diller ve gerekçeleri (TK-01), kütüphaneler ve lisansları, kullanılan çekirdek arayüzleri (PSI, zram, cgroup v2, `memory.reclaim`, freezer) ve neden bunlar | 05-9, 06 | F | 2 |
| 5 | Sistem mimarisi | **Şekil 1** mimari şema; bileşenler; **Şekil 2** cgroup yerleşimi (önce/sonra); IPC protokolü özeti; güvenlik modeli | 05-1, 05-2, 05-6, 05-7 | F (servis), M (ajan, arayüz) | 2,5 |
| 6 | Gerçekleştirim | **Şekil 3** politika durum makinesi; erken sahiplenme ve nedeni (B-07); geçici scope kararı ve deneyi (TK-02, B-08, B-11); güvenli kapanış; profiller; **karşılaşılan problemler** (bulgulardan en az 3'ü: sorun → neden → çözüm) | 05-3, 05-4, 05-5, 08 | F + M | 3 |
| 7 | Test ve değerlendirme | Test ortamı; birim, entegrasyon ve hata durumu testlerinin özet tablosu (T-xx: geçti/kaldı); kabul senaryosu; A–D yöntemi; **Tablo** M1–M6 (ortalama ± sd); **Şekil 4** PSI zaman serisi (C ve D, aynı eksen); **Şekil 5** M1 ve M3 çubuk grafik; hedeflerle karşılaştırma; geçerliliği tehdit eden etkenler | 07, `results/` | M (F, T-01 – T-13'ü yazar) | 3,5 |
| 8 | Grup çalışması | Üye başına geliştirme ve test görevleri tablosu (dosya adlarıyla); kod inceleme süreci; git istatistiği (commit sayısı, satır sayısı, isteğe bağlı) | 04, 10, git | Her üye kendi satırını | 1 |
| 9 | Sonuç ve kısıtlar | Ne çalışıyor; hedefler tuttu mu; bilinen hatalar; kısıtlar (kapsam dışı tablosu, sanal makine, sekme bazında yönetim yok, ajandan önce açılmış uygulamalar); gelecek çalışmalar (GNOME/Wayland, kademe 3, writeback, gerçek donanım, TEKNOFEST) | 03, 08 | F + M | 1 |
| 10 | Kaynaklar ve yapay zekâ kullanımı | Kaynakça; açık kaynak bileşenler ve lisanslar tablosu; yapay zekâ araçları, amaçları ve sınırları | 14, README | M | 1 |

## 2. Şekil ve tablo listesi
| No | İçerik | Üretim | Sorumlu |
|---|---|---|---|
| Şekil 1 | Mimari şema | Aşama 1'deki şema güncellenir: `units.c` kutusu "geçici scope / systemd" olarak, `ses` oku ajandan | M |
| Şekil 2 | cgroup yerleşimi önce/sonra | Gerçek sistemden `systemd-cgls` çıktısının kırpılmış ekran görüntüsü | F |
| Şekil 3 | Politika durum makinesi (0 → 1 → 2 ve geri dönüşler) | draw.io veya elle SVG | F |
| Şekil 4 | `full avg10` ve `some avg10` zaman serisi, C ve D yan yana | `analyze.py` (matplotlib) | M |
| Şekil 5 | M1 ve M3, dört yapılandırma, hata çubuklu | `analyze.py` | M |
| Şekil 6 (isteğe bağlı) | Arayüz ekran görüntüsü, dondurma anında | Ekran görüntüsü | M |
| Tablo 1 | Mevcut araçlar ve fark | 04 | F |
| Tablo 2 | Gereksinimler ve durumları (çalışıyor / kısmen / yok) | 02 + checklist | M |
| Tablo 3 | Test sonuçları özeti | `results/test_sonuclari.md` | M |
| Tablo 4 | M1–M6 sonuçları | `analyze.py` | M |
| Tablo 5 | Görev dağılımı | 10 | İkisi |
| Tablo 6 | Açık kaynak bileşenler ve lisanslar | README | M |

## 3. Yazım kuralları
- Her teknik iddia bir kanıta bağlanır: test kimliği, bulgu kimliği, ölçüm tablosu veya kaynak.
- **Çekirdek davranışı anlatılırken kaynak gösterilir** (çekirdek belgeleri: PSI, cgroup-v2, zram). Sözlü sunumda "nereden biliyorsunuz?" sorusunun cevabı budur.
- **Hedefler tutmadıysa açıkça yazılır.** Değerlendirme ölçütlerinden biri "testlerin ve değerlendirme sonuçlarının yeterliliği"; dürüst ve gerekçeli bir başarısızlık, gizlenmiş bir başarısızlıktan iyidir.
- **Kod parçaları kısa tutulur** (en fazla 15 satır). Yalnızca kritik yerler gösterilir: PSI tetikleyicisi, `memory.reclaim` ve `EAGAIN`, `StartTransientUnit`.
- **Aşama 1'den farklar** "Gerçekleştirim" ve "Kapsam" bölümlerinde açıkça listelenir (Aşama 2: "temel amaç ve özgün katkı korunmalıdır").
- **Kaynakça biçimi:** [n] Yazar/kurum, başlık, yer, yıl, URL, erişim tarihi.

## 4. Kaynakça başlangıç listesi
1. Arpaci-Dusseau, R. H., Arpaci-Dusseau, A. C. *Operating Systems: Three Easy Pieces*, bölüm 21–22 (Beyond Physical Memory).
2. Linux Kernel Documentation, *PSI – Pressure Stall Information* (docs.kernel.org, accounting/psi).
3. Linux Kernel Documentation, *Control Group v2* (docs.kernel.org, admin-guide/cgroup-v2): Memory, Freezer, "Memory Ownership".
4. Linux Kernel Documentation, *zram: Compressed RAM-based block devices* (docs.kernel.org, admin-guide/blockdev/zram).
5. Weiner, J. ve diğ. *TMO: Transparent Memory Offloading in Datacenters*, ASPLOS 2022.
6. facebookincubator/senpai, GitHub.
7. systemd belgeleri: `org.freedesktop.systemd1` D-Bus API (StartTransientUnit, FreezeUnit), `systemd.scope(5)`, `systemd.resource-control(5)`.
8. polkit belgeleri: `polkit(8)`, `org.freedesktop.PolicyKit1.Authority`.
9. Android Open Source Project, *Low Memory Killer Daemon (lmkd)*.
10. earlyoom, nohang, zram-tools, zram-generator proje sayfaları.
11. Pardus 25 sürüm sayfası.

Kaynakçaya eklemeden önce her kaynak açılıp yazar, başlık ve yıl bilgileri doğrulanmalıdır.

## 5. Zamanlama
| Tarih | Teslim |
|---|---|
| 3 Ara | Bölüm 4, 5, 6 (F) ve 7'nin yöntem kısmı (M) |
| 4 Ara | Tüm bölümlerin ilk hali; her üye diğerinin bölümlerini okur ve yorum yazar |
| 6 Ara | Son hali; şekiller yerleştirildi, içindekiler güncellendi, PDF'e çevrildi |
