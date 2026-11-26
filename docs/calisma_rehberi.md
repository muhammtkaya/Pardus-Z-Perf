# Çalışma Rehberi — Kodu Satır Satır Anlamak

Bu rehber, `main` dalındaki **çalışan** kodu takıma öğretmek içindir. Amaç:
sözlü sunumda her üyenin kendi bileşenini satır satır açıklayabilmesi. Kod,
projenin asıl boru hattı sırasıyla gezilir:

> **Güncel belleği/baskıyı algıla → çalışan process'leri algıla → dondur → çöz**

Her aşamada: *hangi dosya*, *hangi fonksiyon*, *hangi çekirdek arayüzü* ve
*neden böyle* anlatılır. Bölüm sonlarında **kendini sına** soruları var;
bunlara akıcı cevap verebiliyorsan o aşamaya hâkimsin.

İlgili kayıt: somut bulgular ve kısıtlar için [bulgular.md](bulgular.md),
IPC protokolü için [ipc-protocol.md](ipc-protocol.md).

---

## 0. Önce bilinmesi gereken kavramlar (yarım gün)

Kodu okumadan önce bu beş çekirdek kavramını bir-iki paragraf düzeyinde öğren:

| Kavram | Tek cümle | Projede nerede |
|---|---|---|
| **PSI** (Pressure Stall Information) | `/proc/pressure/memory` — süreçlerin bellek bekleyerek ne kadar *durakladığı* (`some`/`full`, `avg10`). | baskıyı algılamak |
| **cgroup v2** | Süreçleri bir dizine (`/sys/fs/cgroup/...`) gruplayıp `cgroup.freeze`, `memory.current`, `memory.reclaim` gibi dosyalarla kontrol etme. | gruplama/dondurma/sıkıştırma |
| **zram** | RAM içinde sıkıştırılmış bir takas aygıtı (`/dev/zram0`); soğuk sayfalar diske değil buraya iter. | belleği "yer açar" |
| **Unix soketi + satır-JSON** | `/run/zperf.sock` üzerinden servis ↔ ajan/arayüz haberleşmesi. | IPC |
| **libwnck / GTK** | X11 pencere yöneticisinden odaktaki pencereyi ve PID'ini öğrenme; arayüz çizimi. | ajan + GUI |

> **İpucu:** `man 5 proc` (pressure), çekirdek belgeleri `admin-guide/cgroup-v2`
> ve `admin-guide/blockdev/zram` yeterli arka plandır.

**Bileşen sahipliği (öneriye göre):**
- **Üye 1 — Furkan:** C servisi `zperfd` (Aşama 1, 3, 4'ün servis tarafı + IPC).
- **Üye 2 — Melih:** Python ajan `zperf-agent` + GTK arayüz `zperf-gui` (Aşama 2'nin odak tarafı + çözme butonu).

Yine de **iki üye de tüm boru hattını** bilmeli; aşağıdaki sıra ortak okuma içindir.

---

## Aşama 1 — Güncel belleği/baskıyı algıla

**Soru:** Sistem ne zaman "baskı altında"? Hangi uygulama ne kadar bellek yiyor?

### Okunacaklar
| Dosya | Fonksiyon | Ne yapar |
|---|---|---|
| [psi.c](../src/zperfd/psi.c) | `psi_register`, `psi_read_current`, `psi_avg10` | PSI tetikleyicisi kurar; `/proc/pressure/memory`'yi okur; `some/full avg10` yüzdesini ayrıştırır |
| [main.c](../src/zperfd/main.c) | `classify_tier`, `report_pressure`, ana `epoll` döngüsü | `avg10`'u profil eşikleriyle kıyaslayıp **kademe 0/1/2** belirler |
| [cgroup.c](../src/zperfd/cgroup.c) | `cgroup_app_mem_current` | Bir uygulamanın `memory.current` (bayt) değerini okur |
| [profile.c](../src/zperfd/profile.c) | eşik anahtarları | `tier1_some_pct`, `tier2_some_pct`, `tier2_full_pct` |

### Anahtar fikirler
1. **İki aşamalı PSI:** Çekirdek, tetikleyici eşiğini pencerenin en fazla ~%10'u
   kabul eder (daha yükseği `EINVAL`, bkz. B-M04). Bu yüzden düşük eşikli bir
   **"uyandırma"** tetikleyicisi kurulur; baskı olunca `epoll` `EPOLLPRI` ile
   uyanır, **sonra** `avg10` okunup gerçek kademe hesaplanır.
2. **Kademe eşikleri (Dengeli):** `some avg10 ≥ %10 → K1`, `some ≥ %25 veya
   full ≥ %5 → K2`. Profil değişince eşikler değişir.
3. **Olay-güdümlü, yoklama değil:** `epoll_wait(-1)` sonsuz bekler; CPU yakmaz.
   Ayrıca 5 sn'lik `timerfd` periyodik bakım/durum için uyandırır.

### Kendini sına
- PSI tetikleyici eşiği neden doğrudan "%25" olamıyor?
- `some` ile `full` farkı nedir? Hangisi daha ağır baskı?
- 15 GB RAM'li makinede `avg10` neden hep 0? (bkz. B-M06)

---

## Aşama 2 — Çalışan process'leri algıla

**Soru:** Kullanıcı şu an hangi uygulamada? Hangi süreçler hangi uygulamaya ait?

### Okunacaklar
| Dosya | Fonksiyon | Ne yapar |
|---|---|---|
| [focus.py](../src/agent/zperf_agent/focus.py) | `_on_active_window_changed`, `app_name_from_window` | libwnck `active-window-changed` dinler; pencereden **PID** ve **WM_CLASS**'tan kararlı uygulama adı üretir; servise bildirir |
| [ipc.c](../src/zperfd/ipc.c) | `dispatch_line` (`focus` dalı), `focus_allowed` | Ajandan gelen `{"type":"focus",...}`'u doğrular (kendi PID'in mi?) |
| [policy.c](../src/zperfd/policy.c) | `policy_on_focus` | Uygulamayı **yönetime alır**: cgroup oluştur, süreç ağacını taşı, odak zamanını güncelle |
| [applist.c](../src/zperfd/applist.c) | `applist_add/get`, `app_t` | Yönetilen uygulama kaydı (ad, PID, odak zamanı, bayraklar) |
| [cgroup.c](../src/zperfd/cgroup.c) | `cgroup_app_move_tree` | Uygulamanın tüm süreç ağacını `app-<ad>` cgroup'una taşır |

### Anahtar fikirler
1. **Ad = WM_CLASS'tan türetilen slug** (örn. `google-chrome`); cgroup dizin adı
   olur. Böylece aynı uygulama yeniden odaklanınca aynı grupta bulunur.
2. **Erken gruplama (B-M05):** cgroup v2'de taşınan sürecin *eski* sayfaları yeni
   grupta hemen sayılmaz; bu yüzden uygulama **ilk odakta** gruplanır ki sonradan
   `memory.reclaim` etkili olsun.
3. **Kendi süreçlerini koru (B-M01):** `cgroup_app_move_tree`, `getpid()`'i
   (zperfd) **asla** taşımaz — yoksa servis kendini dondurur.
4. **Güvenlik:** Normal kullanıcı yalnızca **kendi** PID'ini odak olarak bildirebilir
   (`focus_allowed`); PID==0 veren pencereler yönetilmez (B-M09).

### Kendini sına
- Chrome'u açıp tıklayınca servise tam olarak ne gider? (JSON'u yaz)
- `zperfd` neden kendini cgroup'a taşımamalı? Taşısaydı ne olurdu?
- "Süreç ağacı" neden sadece kök PID değil?

---

## Aşama 3 — Dondur (ve sıkıştır)

**Soru:** Baskı yükselince hangi uygulama, nasıl dondurulur?

### Okunacaklar
| Dosya | Fonksiyon | Ne yapar |
|---|---|---|
| [policy.c](../src/zperfd/policy.c) | `policy_on_tier` | K1: odak-dışı uygulamalara küçük `reclaim`. K2: kurban seç → sıkıştır → dondur |
| [policy.c](../src/zperfd/policy.c) | `pick_victim` | Uygun adaylar içinden **o an en çok bellek tüketeni** seçer |
| [cgroup.c](../src/zperfd/cgroup.c) | `cgroup_app_reclaim` | `memory.reclaim`'e bayt yazar (soğuk sayfaları zram'e iter) |
| [cgroup.c](../src/zperfd/cgroup.c) | `cgroup_app_freeze` | `cgroup.freeze`'e `1` yazar (süreçleri durdurur) |

### Anahtar fikirler
1. **Kurban = en çok bellek tüketen** uygun aday (güncel `memory.current`, her
   seferinde okunur). Hariç tutulanlar: odaktaki, "asla dondurma", korunan
   (`ZPERF_PROTECT`), ses çalan, zaten donuk. (Öneriden sapma notu: B-M07.)
2. **Önce sıkıştır, sonra dondur (B-M02):** Donmuş bir cgroup'a `memory.reclaim`
   yazmak olay döngüsünü bloke edebilir; bu yüzden tier-2'de uygulama **canlıyken**
   sıkıştırılır, **ardından** dondurulur.
3. **"Asla dondurma" canlı kontrol:** bayrak her kurban seçiminde okunur; koruma
   kaldırılınca uygulama anında yeniden aday olur.
4. **Olay itme:** Dondurma olunca servis tüm istemcilere `{"event":"frozen"}`
   yollar; ajan bunu masaüstü bildirimine çevirir.

### Kendini sına
- Kademe 2'de iki büyük uygulama varsa hangisi önce donar? Neden?
- Sıralamayı (dondur→sıkıştır) tersine çevirmek neden tehlikeli?
- Odaktaki uygulama neden hiç donmaz?

---

## Aşama 4 — Çöz (thaw)

**Soru:** Donmuş uygulama ne zaman ve nasıl geri gelir?

### Okunacaklar — **dört çözme yolu**
| Yol | Fonksiyon | Tetikleyici |
|---|---|---|
| Odakla | [policy.c](../src/zperfd/policy.c) `policy_on_focus` | Kullanıcı donmuş uygulamaya dönünce **hemen** çözülür (kademe 0) |
| Elle | [policy.c](../src/zperfd/policy.c) `policy_thaw` | Panel **"Çöz"** butonu → `{"type":"thaw"}` |
| Periyodik | [policy.c](../src/zperfd/policy.c) `policy_maintenance` | `thaw_max_frozen_sec`'ten uzun donuk kalanı kısa süre çözer (ağ/ses zaman aşımını önler) |
| Kapanış | [cgroup.c](../src/zperfd/cgroup.c) `cgroup_cleanup_all` | Servis kapanırken/`SIGINT` ile **tümünü** çözer → sistem temiz kalır |

Ayrıca: [policy.c](../src/zperfd/policy.c) `policy_set_enabled(0)` (yönetim
kapatılınca tümünü çöz) ve `policy_set_never_freeze(name,1)` (donuk uygulamayı
pinleyince çöz).

### Anahtar fikirler
1. **Güvenlik önceliği: asla donuk bırakma.** Çıkışta, açılışta (önerilir, I-02)
   ve yönetim kapatılınca her şey çözülür. Kök serviste bir hata bile kullanıcıyı
   kilitli bırakmamalı (R-09).
2. **Çözme = `cgroup.freeze`'e `0` yazmak.** Dondurma gibi ucuz ve anında.
3. **Arayüz isimle çalışır (B-M08):** "Çöz"/"Asla dondurma" o an odaktaki
   uygulamaya değil, **satırdaki isme** etki eder; böylece panele tıklayıp odağı
   GUI'ye almak sorun olmaz.

### Kendini sına
- Donmuş uygulamaya tıklayınca çözülme zincirini baştan sona anlat (ajan→IPC→servis).
- Servis `SIGKILL` ile ölürse donuk uygulamalar ne olur? (İpucu: I-01/B-M01 bölgesi)
- "Asla dondurma"yı donuk bir uygulamaya basınca ne olmalı?

---

## Hâkimiyet planı (kişi başı ~2–3 gün)

| Gün | İş |
|---|---|
| 1 | Bölüm 0 kavramları + Aşama 1 ve 2'yi kodla birlikte oku; "kendini sına"ları cevapla |
| 2 | Aşama 3 ve 4; `gui-test.sh` ile canlı çalıştır, logları ([test-sonuc/](../../test-sonuc/)) olayla eşleştir |
| 3 | Kendi bileşeninde satır satır prova; `bulgular.md`'deki B-M01/B-M03/B-M05'i anlat |

## Sunumda en kritik üç nokta (en iyi "neler öğrendiniz?" cevapları)
- **B-M01:** Servisin kendini dondurma tuzağı ve `getpid()` koruması.
- **B-M03:** polkit "etkin oturum" şartının kök-başlatılan GUI ile sağlanamaması.
- **B-M05:** cgroup v2 bellek sahipliği — süreç taşımak sayfalarını taşımaz.

> Sonraki adım (istenirse): bu rehberdeki noktalara koddan `TODO(anla)` ve
> `TODO(iyilestir)` işaretleri eklemek (bkz. [bulgular.md](bulgular.md) I-01..I-05).
