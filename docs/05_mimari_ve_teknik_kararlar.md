# 05 · Mimari ve Teknik Kararlar

## 1. Genel bakış

```
 Kullanıcı oturumu (UID 1000, X11/XFCE)
 ┌──────────────────────────────┐        ┌──────────────────────────────┐
 │ zperf-agent (Python)         │        │ zperf-gui (Python + GTK 3)   │
 │ libwnck: pencere + odak      │        │ durum, profil, aç/kapat,     │
 │ pactl: ses çalan PID'ler     │        │ "asla dondurma"              │
 │ bildirimler                  │        │                              │
 └──────────────┬───────────────┘        └──────────────┬───────────────┘
                │ satır tabanlı JSON                     │
                └──────────────┬─────────────────────────┘
                               ▼  /run/zperf.sock (SO_PEERCRED; ayar komutları için polkit)
 ┌──────────────────────────────────────────────────────────────────────┐
 │ zperfd (C, root, systemd servisi)                                    │
 │  ipc.c ── apps.c (uygulama tablosu) ── policy.c (kademe durum makinesi)│
 │  psi.c (tetikleyici + avg10)   cgroupfs.c (reclaim, istatistik)       │
 │  units.c (sd-bus: geçici scope, Freeze/Thaw)   zram.c (kurulum)       │
 └───────┬──────────────────────┬──────────────────────┬────────────────┘
         │ poll(POLLPRI)        │ D-Bus (system bus)   │ dosya yazma/okuma
         ▼                      ▼                      ▼
 /proc/pressure/memory   systemd (PID 1)         /sys/fs/cgroup/.../memory.reclaim
                         StartTransientUnit,     /sys/block/zram0/*, swapon(2)
                         FreezeUnit, ThawUnit    /proc/sys/vm/swappiness
```

Tüm döngü tek iş parçacıklıdır: servis bir `poll()` döngüsünde şunları bekler:
- PSI tetikleyici dosya tanımlayıcıları,
- dinleme soketi ve istemci soketleri,
- sinyaller (`signalfd`),
- periyodik zamanlayıcı (`timerfd`, 1 saniye).

Kilit veya iş parçacığı yoktur; bu bilinçli bir sadelik kararıdır.

## 2. cgroup yerleşimi
Ön incelemede (B-05) XFCE'de tüm uygulamaların aynı oturum grubunda olduğu görüldü. Servis, ajanın bildirdiği her uygulamayı systemd aracılığıyla kendi geçici scope'una alır:

```
Önce:  /sys/fs/cgroup/user.slice/user-1000.slice/session-2.scope/
          ├─ xfwm4, xfce4-panel, xfdesktop ...
          ├─ firefox-esr (+ içerik süreçleri)
          └─ mousepad

Sonra: /sys/fs/cgroup/user.slice/user-1000.slice/
          ├─ session-2.scope/                 ← masaüstü bileşenleri burada kalır
          ├─ zperf-firefox-esr-1786.scope/    ← MemoryAccounting=yes
          └─ zperf-mousepad-2401.scope/
```

- **Scope adı:** `zperf-<comm>-<pid>.scope`. `comm`, `/proc/<pid>/comm` değeridir; yalnızca `[a-z0-9_-]` karakterleri bırakılır, en fazla 32 karakter.
- **Slice:** `user-<uid>.slice`. Böylece kullanıcının toplam bellek muhasebesi bozulmaz.
- **Temizlik:** Scope'taki tüm süreçler bitince systemd scope'u kendisi kaldırır (`CollectMode=inactive-or-failed`).
- **Sonradan doğan süreçler:** Uygulamanın daha sonra başlattığı alt süreçler cgroup'u otomatik devralır; servisin onları ayrıca taşıması gerekmez.

## 3. Çalışma akışları

### 3.1 Açılış
1. **Zram kurulumu:** systemd `zperfd`'yi başlatır. `zram.c` zram takası var mı diye bakar (`/proc/swaps`); yoksa sırasıyla şunları yapar:
   - `modprobe zram`,
   - `comp_algorithm` ve `disksize` ayarı,
   - `mkswap`,
   - `swapon(2)` ile öncelik 100,
   - `vm.swappiness` ve `vm.page-cluster` ayarı.
2. **Eski scope'ların çözülmesi:** `units.c`, adı `zperf-` ile başlayan yüklü scope'ları listeler ve hepsini çözer (`ThawUnit`). Önceki çalışmadan donuk kalmış bir şey olmasın (G-08).
3. **PSI tetikleyicileri:** `psi.c`, profildeki eşiklerle iki tetikleyici kurar: K1 ve K2.
4. **Soket:** `ipc.c`, `/run/zperf.sock` dinleme soketini açar.
5. **Hazır bildirimi:** Servis systemd'ye hazır olduğunu bildirir (`sd_notify("READY=1")`, `Type=notify`).

### 3.2 Yeni pencere (erken sahiplenme, TK-03)
```
ajan: window {pid, app}  ──►  zperfd
   zperfd: pid'in UID'i == istemcinin UID'i ve ≥ 1000 mü?     (değilse yok say)
   zperfd: pid sistem bileşeni mi (dışlama listesi)?          (evetse yok say)
   zperfd: pid zaten bir zperf scope'unda mı?                 (evetse yalnızca kaydet)
   zperfd: pid'in oturum grubundaki alt süreç ağacını topla (/proc/*/stat ppid)
   zperfd: StartTransientUnit("zperf-<app>-<pid>.scope", PIDs=[...], Slice, MemoryAccounting=yes)
   zperfd: /proc/<pid>/cgroup scope adını gösterene kadar en fazla 1 sn bekle
   zperfd: uygulama tablosuna ekle {pid, unit, cgroup yolu, son_odak = şimdi}
```

### 3.3 Bellek baskısı yükseliyor
```
çekirdek: K1 tetikleyicisi (POLLPRI) ──► policy: seviye 0 → 1
   her 1 sn (timerfd), seviye ≥ 1 iken:
     odakta olmayan, dışlanmamış her uygulama için:
        adım = min(profil.reclaim_adim, memory.current × %10)
        memory.reclaim ← adım          (EAGAIN = kısmi başarı, hata değil)
çekirdek: K2 tetikleyicisi ──► policy: seviye 2
   aday = odakta değil ∧ dışlanmamış ∧ ses çalmıyor ∧ boşta ≥ profil.bosta_sn ∧ donuk değil
   kurban = adaylar arasında memory.current en büyük olan
   FreezeUnit(kurban.unit) → cgroup.events "frozen 1" olana kadar bekle
   memory.reclaim ← kurban.memory.current × %80   (64 MB'lık parçalarla)
   olay: {frozen, app, geri_kazanilan_mb} → ajan bildirim gösterir
   en fazla 5 saniyede bir yeni dondurma
```

### 3.4 Kullanıcı donmuş uygulamaya dönüyor
Donmuş bir X istemcisi çizim yapamaz, ama pencere yöneticisi (xfwm4) ayrı bir süreç olduğu için odak yine değişir. Bu yüzden ajan odak olayını alır:
```
ajan: focus {pid} ──► zperfd: pid donmuş bir scope'a ait mi?
   evet → ThawUnit(unit); t_thaw = şimdi − t_odak   (M6 için günlüğe)
   uygulama tablosunda son_odak = şimdi
```

### 3.5 Kapanış ve çökme (G-08)
- **SIGTERM/SIGINT:** `signalfd` sinyali okur, `units.c` tüm zperf scope'larını çözer, sonra servis çıkar. Zram açık kalır; çalışan sistemde takası kapatmak tehlikelidir (R-06).
- **Çökme (SIGKILL, segfault):** systemd birimindeki `ExecStopPost=/usr/libexec/zperf/zperf-thaw-all` her durumda çalışır ve tüm `zperf-*.scope` birimlerini çözer. Servis `Restart=on-failure` ile yeniden başlar ve açılışta yine çözer.
- **Politika kapatılırsa:** Aynı çözme işlemi yapılır; scope'lar kalır, sadece kademeler devre dışı olur.

## 4. Politika durum makinesi

| Seviye | Girme koşulu | Çıkma koşulu | Eylem |
|---|---|---|---|
| 0 Normal | Başlangıç | K1 tetiklendi | Yok |
| 1 Proaktif sıkıştırma | K1 tetiklendi | `some avg10` < profil.cikis_esik ve bu 10 sn sürerse → 0; K2 tetiklenirse → 2 | Her saniye arka plan uygulamalarına `memory.reclaim` |
| 2 Dondur ve sıkıştır | K2 tetiklendi | `full avg10` < 1 ve `some avg10` < profil.k1 ve bu 10 sn sürerse → 1 | Seviye 1'in eylemleri, artı en fazla 5 sn'de bir dondurma |

- **Neden iki sinyal?** Tetikleyiciler hızlı tepki için (birkaç yüz ms), `avg10` ise inişte kararlılık (histerezis) için kullanılır. Yalnızca tetikleyiciyle inersek sistem iki seviye arasında gidip gelir (TK-04).
- **Durum makinesi saf bir fonksiyondur:** `policy_step(durum, girdiler, şimdi) → (yeni durum, eylem listesi)`. Böylece çekirdeğe dokunmadan birim testi yazılabilir (bkz. [Kodlama rehberi](06_kodlama_rehberi.md)).

## 5. Profiller (başlangıç değerleri)
Değerler tahmindir. Ölçümlerden sonra **en fazla bir kez** ayarlanır ve gerekçesi raporda yazılır.

| Parametre | Dengeli | Akıllı Tahta | Ofis |
|---|---|---|---|
| `algoritma` | lz4 | lz4 | zstd |
| `swappiness` | 100 | 150 | 100 |
| K1 tetikleyici (`some`, 1 sn pencere) | 100 ms (%10) | 50 ms (%5) | 100 ms (%10) |
| K2 tetikleyici | `some` 250 ms veya `full` 50 ms | `some` 150 ms veya `full` 30 ms | `some` 300 ms veya `full` 80 ms |
| `cikis_esik` (`some avg10`) | 5 | 3 | 5 |
| `reclaim_adim` | 64 MB | 96 MB | 32 MB |
| `bosta_sn` (dondurma için en az boşta kalma) | 60 | 30 | 120 |
| `dondurma` | açık | açık | **kapalı** (yalnızca kademe 1) |

- **Algoritma değişikliği:** Zram aygıtı takas olarak kullanılırken algoritması değiştirilemez. Profil değişince yeni algoritma bir sonraki açılışta geçerli olur ve arayüz bunu kullanıcıya söyler (TK-09).
- **Diğer parametreler:** Hemen geçerli olur; PSI tetikleyicileri kapatılıp yeniden kurulur.

## 6. IPC protokolü (sürüm 1)
- **Taşıma:** `/run/zperf.sock`, `SOCK_STREAM`, izin 0666. Erişim denetimi dosya izniyle değil, her komutta yapılır.
- **Biçim:** Her mesaj tek satır, UTF-8 JSON, `\n` ile biter. En büyük mesaj 64 KB; aşan bağlantı kapatılır.
- **Yanıtlar:** Her istemci komutuna tek satır yanıt verilir. Başarıda `{"ok":true, ...}`, hatada `{"ok":false,"error":"..."}`.

### İstemci → servis
| Komut | Gönderen | Alanlar | Yetki |
|---|---|---|---|
| `hello` | ikisi | `role`: `"agent"` veya `"gui"`, `proto`: 1 | Herkes |
| `window` | ajan | `event`: `"opened"`/`"closed"`, `pid`, `app` | PID'in sahibi istemcinin UID'i olmalı |
| `focus` | ajan | `pid` | Aynı |
| `audio` | ajan | `pids`: [...] | Aynı; listedeki yabancı PID'ler yok sayılır |
| `status` | arayüz | — | Herkes (yalnızca okuma) |
| `subscribe` | ajan | — | Herkes; bağlantıya olay akışı açılır |
| `set_profile` | arayüz | `name` | polkit `io.github.zperf.manage` |
| `set_enabled` | arayüz | `value`: true/false | polkit |
| `never_freeze` | arayüz | `app`, `value` | polkit |

### Servis → istemci olayları (yalnızca `subscribe` eden bağlantıya)
```json
{"event":"frozen","app":"libreoffice","unit":"zperf-soffice-bin-3120.scope","reclaimed_mb":412}
{"event":"thawed","app":"libreoffice","unit":"zperf-soffice-bin-3120.scope","thaw_ms":38}
{"event":"level","from":1,"to":2,"some10":27.4,"full10":6.1}
```

### `status` yanıtı örneği
```json
{"ok":true,"enabled":true,"profile":"dengeli","level":1,
 "psi":{"some10":12.3,"some60":8.0,"full10":0.4},
 "mem":{"total_mb":1987,"available_mb":214},
 "zram":{"orig_mb":610,"compr_mb":188,"used_mb":201,"disksize_mb":993,"algorithm":"lz4"},
 "apps":[{"app":"firefox-esr","unit":"zperf-firefox-esr-1786.scope","mem_mb":512,"swap_mb":230,
          "frozen":false,"focused":true,"idle_s":0,"never_freeze":false,"audio":true}]}
```

## 7. Güvenlik modeli (G-52)
- **Servis root çalışır, saldırı yüzeyi sokettir.** Her bağlantıda istemcinin PID, UID ve GID bilgisi `SO_PEERCRED` ile çekirdekten alınır; istemci bunu taklit edemez.
- **Sahiplik kuralı:** `window`, `focus` ve `audio` komutlarındaki her PID için `/proc/<pid>/status` içindeki gerçek UID, istemcinin UID'ine eşit ve 1000 veya üstü olmalıdır. Böylece bir kullanıcı başka bir kullanıcının ya da root'un uygulamasını dondurtamaz.
- **Ayar komutları:** polkit eylemi `io.github.zperf.manage` için servis, sd-bus üzerinden polkit'in `CheckAuthorization` yöntemini çağırır. Konu (subject) olarak `unix-process` türü, istemcinin PID'i ve başlangıç zamanı verilir. Eylem `allow_active=yes`, `allow_inactive=no`, `allow_any=auth_admin` ile tanımlanır.
- **Dışlama listesi:** Dondurma ve geri kazanım hiçbir zaman şu süreçlere uygulanmaz:
  - Görüntü ve masaüstü: `Xorg`, `xfwm4`, `xfce4-panel`, `xfdesktop`, `xfce4-session`, `xfsettingsd`, `lightdm`.
  - Ses: `pipewire`, `wireplumber`, `pulseaudio`.
  - Kimlik ve yetki: `polkit-gnome-authentication-agent-1` ve `xfce-polkit` gibi polkit ajanları.
  - Projenin kendi bileşenleri: `zperf-agent`, `zperf-gui`.

  Liste `/etc/zperf/zperf.conf` dosyasında genişletilebilir.
- **Girdi güvenliği:** JSON ayrıştırma json-c ile yapılır, alan türleri kontrol edilir. `app` adları scope adına yazılmadan önce temizlenir; dosya yollarına asla istemciden gelen metin konmaz.

## 8. Kurulu sistemdeki dosyalar
| Yol | İçerik |
|---|---|
| `/usr/sbin/zperfd` | Servis |
| `/usr/libexec/zperf/zperf-thaw-all` | Çökmeden sonra çözme betiği (`ExecStopPost`) |
| `/usr/bin/zperf-agent`, `/usr/bin/zperf-gui`, `/usr/bin/zperfctl` | Ajan, arayüz, komut satırı istemcisi |
| `/lib/systemd/system/zperfd.service` | systemd birimi |
| `/etc/xdg/autostart/zperf-agent.desktop` | Ajanın otomatik başlaması |
| `/usr/share/applications/zperf-gui.desktop` | Menüdeki arayüz girdisi |
| `/usr/share/polkit-1/actions/io.github.zperf.policy` | polkit eylemi |
| `/etc/zperf/zperf.conf` | Yapılandırma: profil, dışlama listesi, "asla dondurma" listesi |

`zperf.conf` biçimi (`anahtar = değer`, `#` yorum):
```ini
profile = dengeli
enabled = true
never_freeze = vlc, obs
exclude_extra =
```

## 9. Teknik karar kayıtları

### TK-01 · Servis C ile, istemciler Python ile
- **Karar:** Root servis C11 ile; ajan ve arayüz Python 3 ile yazılır.
- **Gerekçe:**
  - Servis sürekli çalışır ve bellek baskısı altında bile tepki vermelidir. C'de küçük RSS ve öngörülebilir davranış elde edilir.
  - Çekirdek arayüzleri (`poll`, `signalfd`, `swapon`) doğrudan kullanılır.
  - libwnck ve GTK'ya Python'dan erişmek çok daha hızlı geliştirme sağlar.
- **Sonuç:** İki dil, iki üye arasında doğal bir iş bölümü oluşturur.

### TK-02 · Uygulamaları gruplamak için systemd geçici scope'ları
- **Durum: 1. gün deneyle doğrulanacak.**
- **Karar:** Süreçler cgroup dosya sistemine doğrudan yazılarak taşınmaz. Bunun yerine system bus üzerinden `org.freedesktop.systemd1.Manager.StartTransientUnit` çağrılır ve `PIDs` özelliğiyle mevcut süreçler yeni bir scope'a alınır. Dondurma `FreezeUnit`/`ThawUnit` ile yapılır. `memory.reclaim` için systemd'nin API'si olmadığından o dosyaya doğrudan yazılır.
- **Gerekçe:**
  1. systemd kendi yönettiği cgroup ağacının dışarıdan değiştirilmesini desteklemez; elle oluşturulan dallarla çakışabilir.
  2. GNOME uygulamaları zaten bu yöntemle `app-gnome-*.scope` birimlerine koyuyor (B-05). Pardus'un GNOME sürümünde bu düzenle sistemin sorunsuz çalıştığını gördük.
  3. Scope'ların ömrünü systemd yönetir; süreçler bitince scope kendiliğinden silinir.
  4. `MemoryAccounting=yes` ile bellek denetleyicisini systemd açar.
- **Kullanıcı yetkisi yetmez:** Taşıma işlemini root servis yapmalıdır, ajan yapamaz. Kullanıcının kendi systemd yöneticisi süreçleri sistemin oluşturduğu `session-N.scope`'tan çıkaramaz, çünkü kaynak ve hedefin ortak atası root'a aittir. Bu bir çıkarımdır; deneyle doğrulanacak.
- **Doğrulama deneyi:** [Kodlama rehberi](06_kodlama_rehberi.md) bölüm 7.1'deki `busctl` komutlarıyla Mousepad bir scope'a alınır, dondurulur, belleği geri kazanılır ve çözülür. Ardından logind'in uygulamayı nasıl gördüğü ve uygulamanın dosya kaydetme/açma pencereleri kontrol edilir (B-08).
- **Alternatif:** Deney başarısız olursa önerideki yönteme dönülür: `/sys/fs/cgroup/zperf.slice/app-*` dizinleri elle oluşturulur, süreçler `cgroup.procs` dosyasına yazılır, dondurma `cgroup.freeze` ile yapılır. `units.c` aynı arayüzü korur; değişiklik tek dosyada kalır.

### TK-03 · Uygulamalar ilk görüldükleri anda gruplanır
- **Karar:** Ajan her yeni pencereyi bildirir ve servis o uygulamayı hemen kendi scope'una alır. Dondurma anını beklemez.
- **Gerekçe:** cgroup v2'de bir süreç taşındığında, daha önce ayırdığı bellek eski grupta sayılmaya devam eder (B-07). Dondurulmadan hemen önce taşınan bir uygulamada `memory.current` neredeyse sıfır görünür ve `memory.reclaim` işe yaramaz.
- **Sonuç ve kısıt:** Ajan başlamadan önce açılmış uygulamaların önceden ayrılmış belleği oturum grubunda kalır. Ajan oturum açılışında başladığı için bu pratikte küçük bir kayıptır; raporda kısıt olarak yazılır.

### TK-04 · Tepki için PSI tetikleyicisi, iniş için avg10
- **Karar:** Seviye yükseltme PSI tetikleyicileriyle, düşürme `avg10` ve 10 saniyelik bekleme süresiyle yapılır.
- **Gerekçe:** Tetikleyiciler milisaniyeler içinde uyandırır, ama "baskı bitti" bilgisini vermez. `avg10` yavaş değişir, bu yüzden seviyenin gidip gelmesini önler.

### TK-05 · Hiçbir süreç öldürülmez
- **Karar:** Servis hiçbir koşulda `kill` göndermez.
- **Gerekçe:** Projenin farkı budur. Bellek tamamen biterse çekirdeğin OOM killer'ı yine devrededir; M4 ölçümü bunun ne sıklıkla olduğunu gösterir.

### TK-06 · IPC: Unix soketi ve satır tabanlı JSON
- **Karar:** IPC için Unix soketi ve satır tabanlı JSON kullanılır. Servis tarafında json-c, Python tarafında standart `json` modülü.
- **Gerekçe:**
  - D-Bus servisi yazmaktan çok daha az kod gerektirir.
  - `socat` ile elle test edilebilir.
  - `SO_PEERCRED` güvenilir kimlik verir.

### TK-07 · Zram'i servis kurar
- **Karar:** zram-tools'a bağımlı olunmaz; kurulumu servis yapar (`swapon(2)`).
- **Gerekçe:**
  - Profil, algoritma ve boyutun tek yerden yönetilmesi gerekir.
  - zram-tools, ölçümde karşılaştırılan C yapılandırmasıdır; D yapılandırmasında kurulu olmamalıdır.

### TK-08 · Ses algılama ajanda
- **Karar:** Ses çalan uygulamaları ajan bulur: `pactl -f json list sink-inputs` çıktısındaki `application.process.id` alanından.
- **Gerekçe:** Ses sunucusu kullanıcı oturumunda çalışır (ön incelemede PipeWire 1.4 ve PulseAudio uyumluluk katmanı). Ajan aynı oturumda olduğu için doğrudan erişebilir.

### TK-09 · Algoritma değişikliği yeniden başlatmada geçerli olur
- **Karar:** Profil değiştiğinde yeni sıkıştırma algoritması hemen değil, bir sonraki açılışta uygulanır.
- **Gerekçe:** Algoritmayı değiştirmek için zram'i sıfırlamak gerekir, bu da `swapoff` demektir. Bellek doluyken `swapoff` sistemi kilitleyebilir (R-06).

### TK-10 · Paketleme: dpkg-deb ile basit paket
- **Karar:** Paket `make deb` hedefiyle üretilir: önce bir hazırlık dizinine `make install DESTDIR=...` yapılır, sonra `DEBIAN/` kontrol dosyalarıyla `dpkg-deb --build` çalıştırılır. debhelper kullanılmaz.
- **Gerekçe:** 11 günlük takvimde debhelper öğrenmek risklidir. `dpkg-deb` ile kurulabilir bir paket bir saatte hazırlanır.
- **Bakım betikleri:** `postinst` ve `prerm` betikleri şunları yapar:
  - servisi etkinleştirir ve durdurur,
  - kaldırma sırasında zram takasını güvenli biçimde kapatır,
  - `vm.swappiness` değerini eski haline döndürür (G-55).
