# Bulgular ve Riskler (uygulama dalı: `main`)

Bu doküman **yaşayan bir kayıttır**: `main` dalındaki *çalışan* gerçekleştirim
sırasında ve canlı masaüstü testlerinde ortaya çıkan somut bulguları toplar.
Teknik rapordaki "Gerçekleştirim: karşılaşılan problemler" ve "Sonuç ve kısıtlar"
bölümlerinin doğrudan kaynağıdır. (`Real` dalındaki `docs/08_riskler_ve_bulgular.md`
plan/iskelet aşamasının öngörülerini tutar; buradaki bulgular ise koşan koddan
elde edildi.)

**Durum değerleri:** *Doğrulandı* (kanıtı/çalışan kod var) · *Çıkarım* (mantıkla
varılan) · *Doldurulacak* (henüz ölçülmedi).

---

## 1. Gerçekleştirimden çıkan bulgular

| Kimlik | Bulgu | Kaynak / kanıt | Durum | Projeye etkisi |
|---|---|---|---|---|
| B-M01 | **Servis kendini dondurabiliyor.** `zperfd`, odaktaki uygulamanın bir alt süreci olduğunda (ör. test o uygulamanın terminalinden başlatılınca), o uygulamanın süreç ağacını cgroup'a taşırken **kendini de taşıyıp** cgroup donunca kendini dondurur ve tüm olay döngüsü (IPC/bakım/odak) kilitlenir. | Canlı test: `zperfd` `/zperf.slice/app-com-microsoft-vscode` içinde `freeze=1` bulundu | Doğrulandı | Kritik. `cgroup_app_move_tree` artık `getpid()`'i hiçbir uygulama cgroup'una taşımaz ([cgroup.c](../src/zperfd/cgroup.c)). B-07/TK-02'nin somut tezahürü. |
| B-M02 | **Donmuş cgroup'a `memory.reclaim` yazmak olay döngüsünü bloke edebilir** (tek thread'li, senkron yazım). | Canlı test: daemon reclaim yazımında takıldı | Doğrulandı | Tier-1 donmuş cgroup'a reclaim yazmaz; tier-2'de **önce canlıyken sıkıştır, sonra dondur** ([policy.c](../src/zperfd/policy.c)). |
| B-M03 | **polkit `allow_active`, kök-başlatılan GUI ile sağlanamıyor.** Hem `sudo -u` hem `systemd-run --user --scope` süreci logind `session-N.scope` dışına (user@.service/app.slice) koyar; polkit "active oturum" görmez → profil/çöz/asla-dondurma reddedilir. | Test logu: `polkit org.pardus.zperf.manage ... reddedildi` | Doğrulandı | B-05/B-11 doğrulandı. Test için `ZPERF_TRUST_UID` güven-kapısı; **üretimde** (menüden açılan GUI, gerçek oturum) `allow_active=yes` zaten çalışır. |
| B-M04 | **PSI tetikleyici eşiği, pencere süresinin en fazla ~%10'u olabilir;** daha yükseği `EINVAL` verir. Bu yüzden tetikleyici yüksek kademeyi ifade edemez. | Çekirdek kısıtı; kodda doğrulandı ([main.c](../src/zperfd/main.c) PSI kurulumu) | Doğrulandı | Çözüm: düşük eşikli bir "uyandırma" tetikleyicisi kurulur; uyanınca `avg10` okunup kademe hesaplanır. |
| B-M05 | **cgroup v2'de taşınan sürecin eski sayfaları yeni grupta hemen görünmez;** bu yüzden `memory.reclaim` en iyi, uygulama **erken (ilk odakta)** gruplanınca çalışır. Dondurma her koşulda çalışır. | Çekirdek belgesi + kod notu ([policy.c](../src/zperfd/policy.c) `policy_on_focus`) | Çıkarım (B-07) | TK-03: uygulamalar ilk görüldükleri an gruplanır. |
| B-M06 | **Geliştirme makinesinde (15 GB RAM) doğal baskı kademelere ulaşmıyor;** PSI `some/full avg10=0` kalır. | `free -h`, `/proc/pressure/memory` | Doğrulandı | Baskı yapay üretilir: `stress-ng` 400 MB'lık bir cgroup'a (`systemd-run --scope -p MemoryHigh=400M`) hapsedilir. Ölçüm için sanal makine (2 GB) şart (öneri Test düzeneği). |
| B-M07 | **Kurban seçimi "en çok bellek tüketen"e çevrildi** (güncel `memory.current`). Öneri "en uzun süredir odaksız **ve** en çok bellek kullanan" diyordu (LRU+bellek skoru). | Tasarım kararı ([policy.c](../src/zperfd/policy.c) `pick_victim`) | Doğrulandı | **Öneriden sapma** (bilinçli). Raporda gerekçelendirilmeli; istenirse LRU ikincil ölçüt olarak geri eklenebilir. |
| B-M08 | **Arayüzden odak çalma:** eylemler "o an odaktaki uygulama"ya göre yapılırsa, paneli tıklamak odağı GUI'ye alır ve yanlış uygulama hedeflenir. | Canlı test: "asla dondurma" GUI'yi koruyordu | Doğrulandı | Panel **isimle** işlem yapar; servis kendi penceresini (`zperf-gui`) yönetmez/odak saymaz ([window.py](../src/gui/zperf_gui/window.py), [policy.c](../src/zperfd/policy.c)). |
| B-M09 | **`libwnck` bazı pencereler için PID vermez** (`get_pid()==0`). | [focus.py](../src/agent/zperf_agent/focus.py) `_on_active_window_changed` | Çıkarım (R-08/B-13) | Bu pencereler yönetilmez (atlanır); raporda kısıt olarak listelenir. |
| B-M10 | **"Sürücü" uygulamayı korumazsan test kendini yer:** test VSCode/terminalden sürülüyorsa ve o uygulama donarsa, kontrol ortamı da donar. | Canlı test (B-M01 ile aynı kök) | Doğrulandı | `ZPERF_PROTECT` ortam değişkeni: verilen adlar asla dondurulmaz (ayrıca hiç odak çalınmaz). Testte sürücü uygulama bununla korunur. |

---

## 2. İyileştirme listesi (koda eklenen `TODO(iyilestir)`'lerle eşleşir)

| Kimlik | İyileştirme | Dosya | Gerekçe |
|---|---|---|---|
| I-01 | `ExecStopPost` ile **thaw-all** güvenlik ağı + `OOMScoreAdjust=-900` | [systemd/zperfd.service](../systemd/zperfd.service) | Servis `SIGKILL`/çökme ile kapanırsa kendi cleanup'ı çalışmaz; donuk uygulama kalabilir (R-09). |
| I-02 | **Açılışta thaw-all** (önceki çökmeden kalan donuk cgroup'ları çöz) | [main.c](../src/zperfd/main.c) | Çökme sonrası temiz başlangıç. |
| I-03 | **`MemAvailable` tabanlı ek algılama** (yalnız PSI değil) | [policy.c](../src/zperfd/policy.c) | OOM'a yaklaşırken (M4) PSI geç kalabilir. |
| I-04 | Kurban seçiminde **LRU'yu ikincil ölçüt** olarak geri ekleme seçeneği | [policy.c](../src/zperfd/policy.c) | B-M07; öneriyle tam uyum isteniyorsa. |
| I-05 | Ses çalan uygulamayı atlama (TK-08) doğrulaması / `pactl` çıktısı | [audio.c](../src/zperfd/audio.c) | R-04/B-12 doldurulacak. |

---

## 3. Yeni bulgu ekleme şablonu
```markdown
| B-Mxx | (Ne gözlendi, tek cümle) | (log/commit/test kimliği) | Doğrulandı | (etkilenen karar/gereksinim/rapor bölümü) |
```
Her önemli bulgunun kanıtı `test-sonuc/` altında saklanır. Sözlü sunumda
"neler öğrendiniz?" sorusunun en iyi cevapları B-M01, B-M03 ve B-M05'tir.
