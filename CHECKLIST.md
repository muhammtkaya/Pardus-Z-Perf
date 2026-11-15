# Pardus Z-Performans — Yapıldı / Yapılacak Listesi

Son güncelleme: 2026-11-11 (Parça 6 eklendi)
C birim testleri: `sudo tests/run-unit-tests.sh` → **5 geçti, 0 kaldı**
Python: 9 dosya söz dizimi OK + ajan↔servis entegrasyon testi PASS
Test ortamı: WSL2 Ubuntu 24.04, Linux 6.18 (cgroup v2, PSI açık). Nihai doğrulama
hedef sistemde (Pardus 25.0 XFCE) yapılacak.

Durum anahtarı: `[x]` bitti ve test edildi · `[~]` kısmen · `[ ]` yapılmadı

---

## ✅ Tamamlananlar

### Depo iskeleti (Parça 1)
- [x] Klasör düzeni (`src/zperfd`, `src/agent`, `src/gui`, `tests`, `systemd`, …)
- [x] `Makefile` (derleme/çalıştırma/temizleme)
- [x] `README.md`, `.gitignore`
- [x] `systemd/zperfd.service`

### Aşama 1 — `zperfd` iskeleti (öneri 2. hafta çıktısı)
- [x] Seviyeli günlük kaydı — `log.c` (journald uyumlu)
- [x] sysfs/procfs yardımcıları — `util.c`
- [x] **zram kurulumu**: `hot_add` → algoritma → disksize → `mkswap` → `swapon` — `zram.c`
- [x] zram **sökme** (çıkışta temiz) — test edildi: `/proc/swaps` temizleniyor
- [x] Mevcut zram varsa ona dokunmama kuralı
- [x] **PSI izleme**: `/proc/pressure/memory` tetikleyici + `epoll` ile olay-güdümlü uyanma — `psi.c`
- [x] `avg10` ayrıştırma + **kademe sınıflandırma** (0/1/2) — birim testi PASS
- [x] Olay döngüsü: PSI + `signalfd` + `timerfd` (periyodik durum) — `main.c`
- [x] SIGINT/SIGTERM ile temiz kapanış
- [x] Birim testi: `tests/unit/test_psi_parse.c` → PASS

### Parça 4 — cgroup taşıma + dondurma + geri kazanım (öneri 3. hafta çıktısı)
- [x] cgroup v2 tabanı (`zperf.slice`) + memory denetleyicisi — `cgroup.c`
- [x] Uygulama cgroup'u oluşturma (`app-<ad>`)
- [x] Süreç ve **süreç ağacı taşıma** (`/proc` taranarak; çok süreçli uygulamalar)
- [x] **Dondurma / çözme** (`cgroup.freeze`) — test: `frozen=1`/`frozen=0` doğrulandı
- [x] **`memory.reclaim`** ile sayfaları swap'e itme — test: 256 MB → 157 MB
- [x] `memory.current` okuma
- [x] **Güvenlik**: çıkışta `cgroup_cleanup_all()` tüm donmuş uygulamaları çözer
- [x] Uçtan uca mekanizma testi: `tests/unit/test_cgroup.c` → TÜM TESTLER GEÇTİ
- [x] `cgroup_init` + `cleanup` ana servise bağlandı ve entegre test edildi

### Parça 5 — Politika motoru + IPC (öneri 4. hafta çıktısı)
- [x] Uygulama kayıt defteri + **LRU kurban seçimi** — `applist.c`
      (odakta olmayan + en eski + "asla dondurma" değil + donmamış)
- [x] **Politika motoru** — `policy.c`: kademe 1 (küçük adımlı reclaim),
      kademe 2 (kurbanı dondur + sıkıştır), odak değişince çözme
- [x] Baskı kademesi → gerçek eyleme bağlandı (`policy_on_tier`, `main.c`)
- [x] "Asla dondurma" bayrağı (applist düzeyinde)
- [x] **IPC**: `/run/zperf.sock` Unix soketi, satır tabanlı JSON — `ipc.c`
- [x] `SO_PEERCRED` ile istemci kimliği (uid/pid)
- [x] Mesajlar: `focus` (ajandan) ve `status` (JSON durum yanıtı)
- [x] IPC `epoll` ile ana döngüye entegre edildi
- [x] Birim testleri: `test_applist`, `test_policy` (gerçek cgroup), `test_ipc` → hepsi PASS
- [x] Entegre test: servis + istemci → odak→firefox yönetime alındı, durum yanıtlandı, temiz kapanış

### Parça 6 — Ajan + Arayüz (öneri 5. hafta çıktısı)
- [x] Paylaşılan IPC istemcisi — `src/agent/zperf_agent/ipc.py` (saf stdlib)
- [x] **Odak izleyici** — `focus.py` (libwnck ile aktif pencere → ad+PID);
      kararlı uygulama adı WM_CLASS'tan türetiliyor
- [x] Masaüstü bildirim yardımcısı — `notify.py`
- [x] Ajan giriş noktası — `src/agent/main.py`
- [x] **GTK 3 arayüzü** — `src/gui/zperf_gui/window.py`: baskı göstergesi,
      profil seçimi, "odaktaki uygulamayı asla dondurma" düğmesi
- [x] Arayüz giriş noktası — `src/gui/main.py`
- [x] Servise `never_freeze` komutu eklendi (`ipc.c` + `main.c`) ve test edildi
- [x] Python söz dizimi: 9 dosya `py_compile` OK
- [x] **Ajan↔servis entegrasyon testi** (`tests/test_agent_ipc.py`): gerçek
      zperfd'ye odak + never_freeze + status → PASS, servis loglarıyla doğrulandı

> ⚠️ `focus.py` (libwnck/X11) ve `window.py` (GTK ekran) yalnızca söz dizimi
> olarak doğrulandı; gerçek çalışması grafik oturum ister → Pardus XFCE'de test
> edilecek (WSL'de ekran/gi yok).

---

## 🔬 Test bulguları (çekirdek gerçekleri — rapora girecek)

Bu maddeler testlerle ortaya çıktı; öneride öngörülmemişti:

1. **PSI tetikleyici kısıtı:** eşik, pencerenin en fazla **%10'u** olabilir
   (ör. `some 150000 1000000` → `EINVAL`). Bu yüzden "avg10 ≥ %25" bir tetikleyici
   olarak ifade *edilemez*. Çözüm: düşük eşikli "uyandırma" tetikleyicisi + uyanınca
   `avg10` okuyup kademe belirleme.
2. **cgroup v2 yük taşımaz:** var olan sayfalar süreç taşınınca yeni cgroup'a
   yüklenmez; `memory.current` 0 görünür ve reclaim işe yaramaz. **Uygulama,
   belleği ayırmadan önce cgroup'una konmalı** (odak/başlatma anında). → Mimariyi
   etkiler.
3. **`memory.reclaim` + `EAGAIN`:** tam miktar karşılanamazsa kısmen başarsa bile
   `EAGAIN` döner. Hata sayılmamalı; politika küçük adımlarla ilerlemeli.
4. **Algoritma uyumu:** çekirdek istenen algoritmayı (lz4) desteklemiyorsa yazım
   `EINVAL` verir; kod artık etkin algoritmayı geri okuyup raporluyor.
5. **sudo/parola** değil, WSL'de `-u root` ile test edildi (not: geliştirme ortamı).

---

## ⬜ Yapılacaklar

### Parça 5'ten devreden ince ayarlar
- [ ] Ses çalan uygulamaları dondurmadan atlama (PipeWire/PulseAudio sorgusu)
- [ ] Donmuş uygulamayı periyodik kısa süre çözme (ağ/ses zaman aşımı yan etkisi)
- [ ] Kurban seçiminde belleği de hesaba katma (şu an saf LRU)
- [ ] `docs/ipc-protocol.md`: JSON mesaj şemasını belgele

### Parça 6'dan kalan
- [ ] `polkit/org.pardus.zperf.policy` (allow_active=yes) — ayar komutları için yetki
- [ ] `config/profiles/*.conf` (Akıllı Tahta / Ofis / Dengeli) + profil komutu
- [ ] Bildirim itme kanalı: servis → ajan (dondu/çözüldü olayları)
- [ ] Arayüzde aç/kapat ve canlı grafik; profil seçimini servise gönderme
- [ ] Ajanı ve arayüzü gerçek Pardus XFCE/X11 oturumunda çalıştırıp doğrulama

### Parça 7 — Test düzeneği + ölçüm + paketleme (öneri 6–7. hafta)
- [ ] `tests/prober/interaction_prober.c` (M1 takılma ölçeri)
- [ ] `tests/load/workload.sh` (video + LibreOffice + PDF + sekme yükü)
- [ ] `tests/measure/*` (A/B/C/D koşumları, `vmstat`/`mm_stat`/PSI/dmesg toplama)
- [ ] `tests/analysis/analyze.py` (M1–M6 + grafikler)
- [ ] `packaging/debian/*` (.deb paketi)

### Pardus'ta doğrulama (henüz yapılmadı — Pardus kurulu değil)
- [ ] Pardus 25.0 XFCE'yi VirtualBox'a kur (2 GB RAM, 2 vCPU, CPU/disk limiti)
- [ ] `make` ile derle, `sudo ./build/zperfd --debug` ile çalıştır
- [ ] Pardus çekirdeğinde lz4/zstd algoritmalarını doğrula (öneride destekli deniyor)
- [ ] Oturum cgroup'undan taşımanın systemd/logind ile çakışmadığını doğrula
      (risk maddesi; gerekirse D-Bus transient scope'a geç)
- [ ] X11'de bir uygulamayı dondurmanın compositor'ı kilitlemediğini test et

---

## Not: bilinen küçük konular
- Çıkışta boş `zperf.slice` dizini kalır (zararsız; yeniden başlatmada kullanılır).
- Geliştirme WSL2'de yapıldı; WSL çekirdeği lz4 desteklemiyor (Pardus destekliyor).
- **Üst klasör adında boşluk var** ("Pardus Z-Performance") → mutlak yollu derleme
  komutlarında yolun tırnaklanması şart (`-I"$S"`). `make` göreli yol kullandığı
  için etkilenmez. Depo adı `pardus-zperf` bu yüzden boşluksuz seçildi.
