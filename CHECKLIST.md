# Pardus Z-Performans — Yapıldı / Yapılacak Listesi

Son güncelleme: 2026-11-16 — **Pardus 25.2 bare-metal doğrulaması** (çekirdek
6.12.57, XFCE/X11, 16 GB RAM) + IPC yetkilendirme (SO_PEERCRED + polkit) +
.desktop/autostart + ajan yeniden bağlanma + uçtan uca baskı testi + .deb.

Pardus'ta (sudo):
- C birim testleri `sudo tests/run-unit-tests.sh` → **5/5** (test_ipc beklentisi
  düzeltildi; ikinci turda 4/5 idi, kod değil test hatasıydı)
- `tests/test_agent_reconnect.py` (root/X11 gerekmez) → PASS
- **`sudo tests/load/pressure-e2e.sh` → GEÇTİ** (gerçek PSI baskısı, A donduruldu,
  B korundu, polkit + PID sahipliği, temiz kapanış)
- `dpkg-buildpackage -us -uc -b` → `zperf_0.1.0_amd64.deb` BAŞARILI

Önceki geliştirme ortamı WSL2 Ubuntu (Linux 6.18) idi.

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
- [x] **Yetkilendirme (Pardus'ta test edildi)**: soket `0666`; `status` herkese,
      `focus` yalnızca istemcinin KENDİ PID'leri (PID ≤ 1 hep ret), ayar komutları
      root veya polkit `org.pardus.zperf.manage` (`pkcheck`, etkin oturuma parolasız).
      Ad doğrulama `[A-Za-z0-9._-]` (cgroup yol geçişi engeli). Ret → `{"type":"error"}`
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

### Parça 5+6 devreden ayarlar (2026-11-16 — WSL'de derlendi + 5/5 test PASS)
- [x] `docs/ipc-protocol.md`: JSON mesaj şemasını belgele (tüm tipler + event itme)
- [x] `polkit/org.pardus.zperf.policy` (allow_active=yes) — ayar komutları için yetki
- [x] `config/profiles/*.conf` (Dengeli/Ofis/Akıllı Tahta) + `profile` IPC komutu
      — `profile.c/h`: çalışma zamanı ayarlanabilir eşik/reclaim/çözme süresi;
      gömülü varsayilan + varsa `/etc/zperf/profiles/<slug>.conf` ile override
- [x] Bildirim itme kanalı: servis → ajan — `ipc_broadcast` + `{"type":"event"}`;
      ajan `GLib.io_add_watch` ile okuyup masaüstü bildirimi gösterir
- [x] Arayüzde aç/kapat anahtarı + canlı baskı grafiği (cairo sparkline);
      profil seçimi "changed"→servise gönderiliyor; durum profil/enabled yansıtıyor
- [x] Bellek-duyarlı kurban seçimi — `pick_victim` skoru = yaş×(bellek+taban);
      bellek eşit/sıfırsa saf LRU'ya düşer (test_policy hâlâ PASS)
- [x] Donmuş uygulamayı periyodik kısa süre çözme — `policy_maintenance`,
      `frozen_at_ms` + profilin `thaw_max_frozen_sec`'i (durum timer'ında)
- [~] Ses çalan uygulamayı dondurmadan atlama — `audio.c` (pactl sink-inputs,
      corked hariç; best-effort, callback ile enjekte). **Pardus'ta doğrulanacak:**
      zperfd root, kullanıcı PipeWire'ı için XDG_RUNTIME_DIR gerekebilir; kök PID
      eşlemesi alt süreçte çalan sesi kaçırabilir. pactl yoksa 0 → zararsız.

## ⬜ Yapılacaklar

### Pardus XFCE/X11'de çalıştırma (bare metal)
- [~] Ajan + arayüz gerçek XFCE oturumunda: ikisi de normal kullanıcıyla
      açılıyor (çökme yok). **Kalan:** canlı servisle elle doğrulama (libwnck odak,
      event→bildirim, profil/anahtar, "asla dondurma" düğmesi)
- [x] **Soket yetkisi**: önceden `root 0755` idi → normal kullanıcı GUI/ajan
      BAĞLANAMIYORDU (testler root'la koştuğu için görünmüyordu). Düzeltildi.
- [x] `.desktop` dosyaları: `data/zperf-gui.desktop` (menü) +
      `data/zperf-agent.desktop` (`/etc/xdg/autostart`); `desktop-file-validate` temiz
- [x] Ajan yeniden bağlanma: kopunca 3 sn'de bir yeniden dener, olay izlemesini
      yeni sokete taşır, son odağı yeniden gönderir (`tests/test_agent_reconnect.py`)
- [x] İstemci satır tabanlı okuma: `get_status` araya giren event/error satırlarını
      atlıyor (önceden olay satırını durum yanıtı sanabiliyordu)
- [x] GUI polkit reddini durum satırında gösteriyor
- [ ] `audio.c`'yi gerçek PipeWire ile doğrula (gerekirse kök yerine kullanıcı
      oturumundan pactl / XDG_RUNTIME_DIR ayarı)

### Parça 7 — Test düzeneği + ölçüm + paketleme (2026-11-16 — yazıldı + WSL'de doğrulandı)
- [x] `tests/prober/interaction_prober.c` (M1 takılma ölçeri) — derlendi+çalıştı
      (Pardus GCC 14: `qsort` karşılaştırıcısı `long`→`int` düzeltildi);
      CLOCK_MONOTONIC/TIMER_ABSTIME ile uyanma gecikmesi, CSV + p50/p95/p99/max özet
- [x] `tests/load/workload.sh` — sentetik (stress-ng/python ayırıcı) + `--apps` (gerçek
      Firefox/LibreOffice/Evince) kipleri; `bash -n` temiz
- [x] `tests/measure/run.sh` — A/B/C/D senaryoları, collector (PSI/mem+pswpin/zram
      mm_stat/zperfd CPU-RSS/dmesg-OOM), profil D için IPC ile set; `bash -n` temiz
- [x] `tests/analysis/analyze.py` — M1–M6 hesap + tablo + matplotlib grafikleri
      (PSI zaman serisi, M1 bar, CDF); sahte veriyle doğrulandı, py_compile OK
- [x] `packaging/debian/*` (.deb) + `Makefile install`/`prober` hedefleri —
      `make install DESTDIR=...` 16 dosyayı doğru yollara kurdu (pycache hariç)

> ⚠️ Parça 7 kodu yazıldı/derlendi ama **gerçek ölçüm Pardus'ta** yapılacak:
> `sudo tests/measure/run.sh A|B|C|D` → `analyze.py data/*/ --out rapor/`.
> M1–M6 tanımları dosya başlıklarında; nihai rapordaki ifadelerle uyumlanacak.

### Pardus'ta doğrulama (bare metal — Pardus artık kurulu)
- [x] `make` ile derle (GCC 14, uyarısız; dpkg sertleştirme bayraklarıyla da),
      `sudo ./build/zperfd --debug` ile çalıştır
- [x] Pardus çekirdeğinde algoritmalar: `lz4 lz4hc zstd deflate` mevcut; lz4 etkin
- [x] Boştaki `zram0` sahipleniliyor (önceden `modprobe` zram0'ı boş bırakıp zram1 açılıyordu)
- [x] **Gerçek baskı testi** (`tests/load/pressure-e2e.sh`; stress-ng `MemoryHigh=400M`
      scope'ta): some avg10 en yüksek %46.6, BASKI OLAYI alındı, kademe 2'de en eski
      uygulama (A) donduruldu ve 300 MB → 0 MB sıkıştırıldı, odaktaki B hiç donmadı,
      `frozen` olayı yayınlandı, kapanışta zram/soket/donmuş cgroup kalmadı
- [x] PSI uyanış fırtınası: baskıda ~18 uyanış/sn ölçüldü → politika en fazla
      1/sn çalışacak şekilde birleştirildi; < 4 MB belleği olan uygulamaya reclaim yok
- [ ] Oturum cgroup'undan taşımanın systemd/logind ile çakışmadığını doğrula
      (risk maddesi; gerekirse D-Bus transient scope'a geç)
- [ ] X11'de bir uygulamayı dondurmanın compositor'ı kilitlemediğini test et
- [ ] `sudo tests/measure/run.sh` ile A/B/C/D ölçümlerini al, `analyze.py` ile M1–M6
- [x] `.deb` üret (`ln -sfn packaging/debian debian && dpkg-buildpackage -us -uc -b`)
      → BAŞARILI. Düzeltilenler: dpkg `CFLAGS` `-D_GNU_SOURCE`'u eziyordu (derleme
      kırılıyordu); Depends'e `python3-gi-cairo` (grafik), `polkitd` (pkcheck),
      `kmod`, `util-linux` eklendi; `gir1.2-notify-0.7` Recommends
- [ ] `.deb`'i temiz bir Pardus'a kurup servis + autostart + menüyü doğrula

---

## Not: bilinen küçük konular
- Depo Windows'tan yüklendiği için betiklerde çalıştırma izni yoktu; git'te `+x` verildi.
- `test_cgroup` reclaim adımı takas ister; zram yoksa test geçici zram kurar.
- Çıkışta boş `zperf.slice` dizini kalır (zararsız; yeniden başlatmada kullanılır).
- Geliştirme WSL2'de yapıldı; WSL çekirdeği lz4 desteklemiyor (Pardus destekliyor).
- **Üst klasör adında boşluk var** ("Pardus Z-Performance") → mutlak yollu derleme
  komutlarında yolun tırnaklanması şart (`-I"$S"`). `make` göreli yol kullandığı
  için etkilenmez. Depo adı `pardus-zperf` bu yüzden boşluksuz seçildi.
