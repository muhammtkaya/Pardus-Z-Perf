# 06 · Kodlama Rehberi

Bu doküman kodu yazarken açık durmalı. Kod örnekleri doğru API'leri ve doğru sırayı göstermek içindir. Hata denetimleri kısaltılmıştır; derleyip sanal makinede denemeden kullanmayın.

## 1. Geliştirme ortamı
Geliştirme ve test, Pardus 25 XFCE sanal makinesinde yapılır (ön incelemedeki makine; "temiz-kurulum" anlık görüntüsü alınmış olmalı).

```bash
sudo apt install -y build-essential pkg-config git libsystemd-dev libjson-c-dev \
  python3-gi gir1.2-gtk-3.0 gir1.2-wnck-3.0 gir1.2-notify-0.7 \
  pulseaudio-utils socat stress-ng
```
- `gir1.2-notify-0.7` ve `socat` paketlerinin Pardus deposunda olduğu henüz doğrulanmadı; ilk gün `apt-cache policy` ile kontrol edin.
- Bildirim paketi yoksa bildirimler `notify-send` ile gösterilir.

## 2. Depo yapısı
Kod `Real` dalındaki `source_code/` klasörüne yazılır; bu klasör teslim zip'ine olduğu gibi kopyalanır. `main` dalındaki eski kod kullanılmaz ve oradan dosya kopyalanmaz.

```
source_code/
├── Makefile                 make, make test, make install, make deb, make clean
├── LICENSE                  GPL-3.0 tam metni
├── daemon/
│   ├── src/
│   │   ├── main.c           başlatma, poll döngüsü, signalfd, timerfd, sd_notify
│   │   ├── config.c/.h      /etc/zperf/zperf.conf okuma/yazma, profil tablosu
│   │   ├── psi.c/.h         tetikleyici kurma, avg10 okuma
│   │   ├── zram.c/.h        zram kurulumu, istatistik
│   │   ├── cgroupfs.c/.h    memory.current/swap.current/reclaim, pid → cgroup yolu
│   │   ├── units.c/.h       sd-bus: geçici scope, Freeze/Thaw, thaw-all
│   │   ├── apps.c/.h        uygulama tablosu (pid, unit, son odak, bayraklar)
│   │   ├── policy.c/.h      saf durum makinesi: policy_step()
│   │   ├── ipc.c/.h         Unix soket sunucusu, JSON komutları
│   │   ├── auth.c/.h        SO_PEERCRED, UID denetimi, polkit CheckAuthorization
│   │   └── util.c/.h, log.h dosya yazma/okuma yardımcıları, günlük makroları
│   └── tests/
│       ├── test_policy.c    policy_step birim testleri
│       └── test_config.c
├── agent/zperf-agent        Python, çalıştırılabilir
├── gui/zperf-gui            Python + GTK 3
├── gui/zperf-gui.desktop
├── cli/zperfctl             Python komut satırı istemcisi (test ve yedek arayüz)
├── config/zperf.conf        varsayılan yapılandırma
├── systemd/zperfd.service
├── polkit/io.github.zperf.policy
├── xdg/zperf-agent.desktop  autostart
├── scripts/zperf-thaw-all
├── packaging/DEBIAN/        control, postinst, prerm, postrm
└── tools/
    ├── probe/probe.c        etkileşim yoklayıcısı (M1, M3)
    ├── bench/collect.py     veri toplayıcı (M2, M5, M6)
    ├── bench/load.sh        yük betiği
    ├── bench/tab.html       bellek tüketen sentetik sekme
    ├── bench/analyze.py     M1–M6 tabloları ve grafikler
    └── on_inceleme.sh
```

**Teslimde bulunmaması gerekenler** (Aşama 2, 2.1): derleme çıktıları (`*.o`, `zperfd` ikilisi, `.deb`), ham ölçüm CSV'lerinin büyükleri (yalnızca özet tablo ve grafik). `.gitignore` buna göre yazılır.

## 3. Derleme
`Makefile` hedefleri:

| Hedef | Ne yapar |
|---|---|
| `make` | `daemon/zperfd` ve `tools/probe/probe` derlenir |
| `make test` | Birim testleri derlenir ve çalıştırılır |
| `sudo make install` | Dosyalar bölüm 05-8'deki yollara kurulur; `PREFIX` ve `DESTDIR` desteklenir |
| `sudo make uninstall` | Kurulan dosyalar kaldırılır |
| `make deb` | `build/deb/` altına kurulum yapılır, `dpkg-deb --build` ile `zperf_<sürüm>_amd64.deb` üretilir |
| `make clean` | Derleme çıktıları silinir |

Derleyici bayrakları:
```make
CFLAGS += -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Wshadow -Wformat=2
CFLAGS += $(shell pkg-config --cflags libsystemd json-c)
LDLIBS += $(shell pkg-config --libs libsystemd json-c)
```
Geliştirme sırasında ayrıca `make SAN=1` ile `-fsanitize=address,undefined` açılır.

## 4. Kodlama kuralları

### C (servis, yoklayıcı)
- **Adlandırma:** Dışa açık fonksiyonlar modül önekiyle başlar: `psi_`, `zram_`, `cg_`, `units_`, `apps_`, `policy_`, `ipc_`, `auth_`, `cfg_`. Statik yardımcılar öneksizdir.
- **Hata dönüşü:** Başarıda `0` veya pozitif değer, hatada `-errno` döner. `errno` çağıran tarafta okunmaz; hata değer olarak taşınır.
- **Global durum yok:** Tüm durum `main.c`'deki tek bir `struct zp_ctx` içindedir ve fonksiyonlara işaretçiyle verilir. Statik değişken yalnızca sabit tablolar için kullanılır.
- **Bellek:** Döngü içinde `malloc` yapılmaz. Uygulama tablosu sabit boyutlu bir dizidir (`ZP_MAX_APPS 128`). Servis bellek baskısı anında da çalışmak zorunda.
- **Dosya yazma:** Her `/proc` ve `/sys` yazımı tek yardımcıdan geçer: `util_write_file(path, str)` bir kerede `write()` yapar, kısmi yazımı hata sayar. Bu sayede hatalar tek yerde günlüğe düşer.
- **Günlük:** journald'ın tanıdığı düzey önekleri kullanılır:
  ```c
  #define LOG_ERR(fmt, ...)  fprintf(stderr, "<3>" fmt "\n", ##__VA_ARGS__)
  #define LOG_WARN(fmt, ...) fprintf(stderr, "<4>" fmt "\n", ##__VA_ARGS__)
  #define LOG_INFO(fmt, ...) fprintf(stderr, "<6>" fmt "\n", ##__VA_ARGS__)
  #define LOG_DBG(fmt, ...)  do { if (g_debug) fprintf(stderr, "<7>" fmt "\n", ##__VA_ARGS__); } while (0)
  ```
  Ölçümde ayrıştırılacak olaylar sabit biçimde yazılır, örneğin:
  ```
  EVT frozen app=libreoffice unit=zperf-soffice-bin-3120.scope reclaimed_mb=412 t=1764150000.123
  ```
- **Yorumlar:** Türkçe. "Ne yaptığını" değil, "neden" yaptığını anlatır; özellikle çekirdek davranışına dayanan yerlerde (örn. `EAGAIN` neden hata değil).
- **Biçim:** 4 boşluk girinti, satır en fazla 100 karakter. İstenirse `clang-format` (LLVM stili, `IndentWidth: 4`).

### Python (ajan, arayüz, zperfctl, betikler)
- PEP 8, tür ipuçları, `#!/usr/bin/env python3`.
- Yalnızca Debian paketleriyle gelen modüller kullanılır; `pip` bağımlılığı yok.
- Ajan ve arayüz GLib ana döngüsünde çalışır. Soket okuma `GLib.io_add_watch` ile yapılır; arayüz donmamalı.
- Her dosyanın başında 3–5 satırlık bir açıklama yorumu bulunur.

### Genel
- Her commit tek bir şey yapar. Mesaj biçimi: `bileşen: açıklama (G-xx)`. Örnek: `daemon: kademe 2 kurban seçimi (G-06)`.
- **Kod incelemesi:** Önerideki taahhüt gereği her üye diğerinin kodunu inceler. `main` dalına doğrudan yazılmaz; kısa ömürlü dallar ve GitHub Pull Request kullanılır. İnceleme en geç ertesi sabah yapılır. Sözlü sunumda "birbirimizin kodunu biliyoruz" demenin kanıtı budur.
- **Yapay zekâ ile yazılan veya düzeltilen her kod** [Yapay zekâ kullanım kaydı](14_yapay_zeka_kullanim_kaydi.md)'na bir satır olarak işlenir ve yazan kişi tarafından satır satır anlaşılmadan birleştirilmez.

## 5. Servis modülleri ve arayüzleri

```c
/* psi.h */
int  psi_trigger_open(const char *spec);          /* "some 100000 1000000" → fd (POLLPRI) */
int  psi_read_avgs(struct psi_avgs *out);         /* some/full avg10, avg60 */

/* zram.h */
int  zram_setup(const struct zp_profile *p);      /* var olanı kullanır; yoksa kurar */
int  zram_stats(struct zram_stats *out);          /* mm_stat ilk üç alan, disksize, algoritma */

/* cgroupfs.h */
int  cg_path_of_pid(pid_t pid, char *buf, size_t n);   /* /proc/<pid>/cgroup "0::" satırı */
int  cg_read_u64(const char *cg, const char *file, uint64_t *out);
int  cg_reclaim(const char *cg, uint64_t bytes);        /* EAGAIN → 0 */
int  cg_wait_frozen(const char *cg, int timeout_ms);    /* cgroup.events "frozen 1" */

/* units.h */
int  units_adopt(sd_bus *b, const char *unit, const pid_t *pids, size_t n, uid_t uid);
int  units_freeze(sd_bus *b, const char *unit);
int  units_thaw(sd_bus *b, const char *unit);
int  units_thaw_all(sd_bus *b);                   /* "zperf-*.scope" */

/* apps.h */
struct zp_app *apps_find_pid(struct zp_apps *t, pid_t pid);   /* pid veya ağaçtaki ata */
struct zp_app *apps_add(struct zp_apps *t, pid_t pid, const char *comm, uid_t uid);
void           apps_remove_dead(struct zp_apps *t);           /* scope'u kalmayanlar */

/* policy.h: saf, yan etkisiz */
struct policy_input  { int k1_fired, k2_fired; struct psi_avgs psi; double now;
                       const struct zp_app *apps; size_t napps; int focused_idx; };
struct policy_action { enum { ACT_RECLAIM, ACT_FREEZE } kind; int app_idx; uint64_t bytes; };
int policy_step(struct policy_state *s, const struct zp_profile *p,
                const struct policy_input *in,
                struct policy_action *out, size_t max_out);   /* eylem sayısını döndürür */
```

`policy_step` dosya okumaz, D-Bus çağırmaz, saati kendisi almaz. Tüm girdiler parametredir. Bu yüzden `test_policy.c` sahte girdilerle şunları test edebilir:
- K1 ateşlenince seviye 1'e geçiş,
- histerezis,
- odaktaki uygulamanın asla seçilmemesi,
- boşta kalma süresi,
- 5 saniyelik dondurma aralığı.

## 6. Çekirdek ve systemd arayüzleri: örnek kodlar

### 6.1 PSI tetikleyicisi
```c
/* Kaynak: çekirdek belgeleri, accounting/psi. Her tetikleyici kendi fd'sini ister. */
int psi_trigger_open(const char *spec)
{
    int fd = open("/proc/pressure/memory", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    /* Çekirdek örneği NUL dahil yazar. Pencere 500 ms – 10 s arası olmalı. */
    if (write(fd, spec, strlen(spec) + 1) < 0) {
        int e = errno;
        close(fd);
        return -e;
    }
    return fd;   /* poll(): eşik aşılınca POLLPRI; POLLERR = PSI desteklenmiyor */
}
```
`psi_read_avgs` aynı dosyayı **ayrı bir fd ile** okur. Tetikleyici fd'sinden okuma yapılmaz.

### 6.2 Ana döngü iskeleti
```c
sigset_t m; sigemptyset(&m); sigaddset(&m, SIGTERM); sigaddset(&m, SIGINT);
sigprocmask(SIG_BLOCK, &m, NULL);
int sfd = signalfd(-1, &m, SFD_CLOEXEC);
int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);   /* 1 sn periyot */

struct pollfd p[ZP_MAX_FDS];
/* p[0]=sfd, p[1]=tfd, p[2]=k1, p[3]=k2, p[4]=dinleme soketi, p[5..]=istemciler */
for (;;) {
    poll(p, n, -1);
    if (p[0].revents & POLLIN) break;                       /* temiz kapanış */
    if (p[2].revents & POLLPRI) in.k1_fired = 1;
    if (p[3].revents & POLLPRI) in.k2_fired = 1;
    if (p[1].revents & POLLIN) { read_timer(tfd); tick = 1; }
    /* soket olayları → ipc_handle(), odak değişince units_thaw() hemen */
    if (tick || in.k1_fired || in.k2_fired) {
        fill_input(&ctx, &in);                  /* psi avg, apps, şimdi */
        n_act = policy_step(&ctx.pol, ctx.prof, &in, act, ZP_MAX_ACT);
        apply_actions(&ctx, act, n_act);        /* reclaim / freeze */
        in.k1_fired = in.k2_fired = tick = 0;
    }
}
units_thaw_all(ctx.bus);                        /* G-08 */
```

### 6.3 Süreç ağacını toplama
```c
/* /proc/<pid>/stat: "pid (comm) state ppid ...". comm boşluk ve ')' içerebilir,
   bu yüzden SON ')' karakterinden sonrası ayrıştırılır. */
static pid_t read_ppid(pid_t pid)
{
    char path[64], buf[512];
    snprintf(path, sizeof path, "/proc/%d/stat", pid);
    if (util_read_file(path, buf, sizeof buf) < 0) return -1;
    char *rp = strrchr(buf, ')');
    char state; int ppid;
    if (!rp || sscanf(rp + 2, "%c %d", &state, &ppid) != 2) return -1;
    return ppid;
}
```
- Kök PID'in torunlarını bulmak için `/proc` bir kez taranır ve her sürecin ebeveyni okunur. Ebeveyn zinciri köke çıkan süreçler toplanır.
- Yalnızca kökle **aynı cgroup'ta** olanlar alınır, böylece başka bir scope'taki süreç yerinden oynatılmaz.

### 6.4 Geçici scope oluşturma (TK-02)
```c
#define CHK(x) do { r = (x); if (r < 0) goto out; } while (0)

int units_adopt(sd_bus *bus, const char *unit, const pid_t *pids, size_t n, uid_t uid)
{
    sd_bus_message *m = NULL, *reply = NULL;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    char slice[64];
    int r;

    snprintf(slice, sizeof slice, "user-%u.slice", (unsigned) uid);
    CHK(sd_bus_message_new_method_call(bus, &m, "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
            "StartTransientUnit"));
    CHK(sd_bus_message_append(m, "ss", unit, "fail"));
    CHK(sd_bus_message_open_container(m, 'a', "(sv)"));
    CHK(sd_bus_message_append(m, "(sv)", "Slice", "s", slice));
    CHK(sd_bus_message_append(m, "(sv)", "MemoryAccounting", "b", 1));
    CHK(sd_bus_message_append(m, "(sv)", "CollectMode", "s", "inactive-or-failed"));
    CHK(sd_bus_message_append(m, "(sv)", "Description", "s", "Z-Performans uygulama kapsami"));
    /* PIDs: a(sv) içinde ("PIDs", <au>) */
    CHK(sd_bus_message_open_container(m, 'r', "sv"));
    CHK(sd_bus_message_append(m, "s", "PIDs"));
    CHK(sd_bus_message_open_container(m, 'v', "au"));
    CHK(sd_bus_message_open_container(m, 'a', "u"));
    for (size_t i = 0; i < n; i++)
        CHK(sd_bus_message_append(m, "u", (uint32_t) pids[i]));
    CHK(sd_bus_message_close_container(m));   /* a */
    CHK(sd_bus_message_close_container(m));   /* v */
    CHK(sd_bus_message_close_container(m));   /* r */
    CHK(sd_bus_message_close_container(m));   /* a(sv) */
    CHK(sd_bus_message_append(m, "a(sa(sv))", 0));
    CHK(sd_bus_call(bus, m, 0, &err, &reply));
    /* Dönen değer bir iş (job) yoludur; taşıma eşzamansızdır. Çağıran taraf
       /proc/<pid>/cgroup scope adını gösterene kadar (en fazla 1 sn) bekler. */
out:
    if (r < 0) LOG_ERR("adopt %s: %s", unit, err.message ? err.message : strerror(-r));
    sd_bus_error_free(&err);
    sd_bus_message_unref(m);
    sd_bus_message_unref(reply);
    return r;
}
```

### 6.5 Dondurma ve çözme
```c
int units_freeze(sd_bus *bus, const char *unit)
{
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int r = sd_bus_call_method(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                               "org.freedesktop.systemd1.Manager", "FreezeUnit",
                               &err, NULL, "s", unit);
    sd_bus_error_free(&err);
    return r;     /* ThawUnit aynı biçimde */
}
```
`FreezeUnit` da eşzamansız tamamlanabilir. Geri kazanıma başlamadan önce `cg_wait_frozen()` ile `cgroup.events` dosyasında `frozen 1` görülene kadar beklenir (en fazla 500 ms).

### 6.6 Proaktif geri kazanım
```c
int cg_reclaim(const char *cg, uint64_t bytes)
{
    char path[PATH_MAX], val[32];
    snprintf(path, sizeof path, "/sys/fs/cgroup%s/memory.reclaim", cg);
    snprintf(val, sizeof val, "%" PRIu64, bytes);
    int r = util_write_file(path, val);
    /* EAGAIN: çekirdek istenen miktarın tamamını geri kazanamadı. Kısmi başarı
       normaldir (örn. zram doldu, sayfalar sıcak); hata sayılmaz. */
    return r == -EAGAIN ? 0 : r;
}
```
Geri kazanılan miktar, öncesi ve sonrası `memory.current` farkından hesaplanıp günlüğe yazılır.

### 6.7 Zram kurulumu
```c
/* Sıra önemli: algoritma, disksize'dan ÖNCE yazılmalı. */
util_run((char *[]){ "/usr/sbin/modprobe", "zram", NULL });
util_write_file("/sys/block/zram0/comp_algorithm", p->algorithm);   /* "lz4" */
util_write_file("/sys/block/zram0/disksize", disksize_str);         /* RAM'in %50'si */
util_run((char *[]){ "/usr/sbin/mkswap", "/dev/zram0", NULL });
swapon("/dev/zram0", SWAP_FLAG_PREFER |
       ((100 << SWAP_FLAG_PRIO_SHIFT) & SWAP_FLAG_PRIO_MASK));     /* <sys/swap.h> */
util_write_file("/proc/sys/vm/swappiness", "100");
util_write_file("/proc/sys/vm/page-cluster", "0");
```
- **Mevcut zram:** `/proc/swaps` içinde `/dev/zram` varsa kurulum atlanır.
- **zram0 meşgulse:** `/sys/class/zram-control/hot_add` okunarak yeni bir aygıt numarası alınır.
- **Eski değerler:** İlk değiştirmeden önce eski `swappiness` değeri `/var/lib/zperf/swappiness.orig` dosyasına yazılır. Paket kaldırılırken `postrm` bu değeri geri yükler (G-55).

### 6.8 polkit yetki sorgusu
```c
/* Konu: istemci süreç. start-time = /proc/<pid>/stat alan 22 (açılıştan beri tick). */
r = sd_bus_call_method(bus, "org.freedesktop.PolicyKit1",
        "/org/freedesktop/PolicyKit1/Authority", "org.freedesktop.PolicyKit1.Authority",
        "CheckAuthorization", &err, &reply, "(sa{sv})sa{ss}us",
        "unix-process", 2, "pid", "u", (uint32_t) pid, "start-time", "t", start_time,
        "io.github.zperf.manage",
        0,          /* details: boş */
        1,          /* AllowUserInteraction */
        "");
int authorized = 0, challenge = 0;
sd_bus_message_enter_container(reply, 'r', "bba{ss}");
sd_bus_message_read(reply, "bb", &authorized, &challenge);
```

## 7. Doğrulama deneyleri (ilk gün, kodlamadan önce)

### 7.1 TK-02: geçici scope + dondurma + geri kazanım
Komutları sanal makinede, bir terminalde çalıştırın.
1. Mousepad'i menüden açın, **dosya açmadan**. PID'i ve şu anki grubunu alın:
   ```bash
   PID=$(pgrep -n -x mousepad); cat /proc/$PID/cgroup
   ```
2. Mousepad'i bir geçici scope'a alın:
   ```bash
   sudo busctl call org.freedesktop.systemd1 /org/freedesktop/systemd1 \
     org.freedesktop.systemd1.Manager StartTransientUnit 'ssa(sv)a(sa(sv))' \
     zperf-deneme.scope fail 3 PIDs au 1 $PID Slice s user-1000.slice MemoryAccounting b true 0
   ```
3. Taşındığını ve bellek denetleyicisinin açıldığını kontrol edin. Ardından Mousepad'de 20 MB'lık `~/buyuk.txt` dosyasını açın:
   ```bash
   cat /proc/$PID/cgroup
   CG=/sys/fs/cgroup/user.slice/user-1000.slice/zperf-deneme.scope
   ls $CG/memory.reclaim
   ```
4. Dondurun, belleği geri kazanın, çözün:
   ```bash
   sudo systemctl freeze zperf-deneme.scope
   systemctl show -p FreezerState zperf-deneme.scope
   echo 50M | sudo tee $CG/memory.reclaim; cat $CG/memory.swap.current
   sudo systemctl thaw zperf-deneme.scope
   ```
5. logind'in sürece nasıl baktığını kontrol edin:
   ```bash
   busctl call org.freedesktop.login1 /org/freedesktop/login1 \
     org.freedesktop.login1.Manager GetSessionByPID u $PID
   ```
6. Mousepad'de **Kaydet** ve **Aç** pencerelerini deneyin.

**Kayıt:** Her adımın çıktısını [Bulgular](08_riskler_ve_bulgular.md) B-08 ve B-11'e yazın.
- **Deney başarılıysa:** TK-02 "kabul edildi" olur.
- **2. adım reddedilirse:** TK-02 alternatifine geçilir ve aynı adımlar `/sys/fs/cgroup/zperf.slice` ile tekrarlanır.

### 7.2 PSI tetikleyicisi
Bölüm 6.1'deki kodu 20 satırlık bir programla derleyin ve yük altında uyandığını görün:
```bash
stress-ng --vm 2 --vm-bytes 90% --timeout 60s
```

### 7.3 libwnck
Odak her değiştiğinde pencere başlığını, `get_pid()` sonucunu ve `/proc/<pid>/comm` değerini yazdıran bir betik yazın. `get_pid()` sonucunun 0 döndüğü uygulamaları not edin; bunlar yönetilemeyecek uygulamalardır ve kısıt olarak raporlanır.

## 8. Ajan iskeleti
```python
#!/usr/bin/env python3
"""zperf-agent: odak, yeni pencere ve ses bilgisini zperfd'ye iletir; olayları bildirir."""
import json, os, socket, subprocess
import gi
gi.require_version("Gtk", "3.0"); gi.require_version("Wnck", "3.0")
from gi.repository import GLib, Gtk, Wnck

SOCK = "/run/zperf.sock"

class Agent:
    def __init__(self) -> None:
        self.sock: socket.socket | None = None
        self.screen = Wnck.Screen.get_default()
        self.screen.force_update()
        self.screen.connect("window-opened", lambda s, w: self.send_window(w, "opened"))
        self.screen.connect("window-closed", lambda s, w: self.send_window(w, "closed"))
        self.screen.connect("active-window-changed", self.on_focus)
        GLib.timeout_add_seconds(5, self.poll_audio)
        self.connect()

    def connect(self) -> bool:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(SOCK)
            self.sock = s
            self.send({"cmd": "hello", "role": "agent", "proto": 1})
            self.send({"cmd": "subscribe"})
            for w in self.screen.get_windows():          # G-25: yeniden bağlanınca tam liste
                self.send_window(w, "opened")
            GLib.io_add_watch(s.fileno(), GLib.IO_IN | GLib.IO_HUP, self.on_data)
            return False                                  # yeniden deneme zamanlayıcısını durdur
        except OSError:
            GLib.timeout_add_seconds(2, self.connect)
            return False

    def send(self, msg: dict) -> None:
        if self.sock:
            try:
                self.sock.sendall((json.dumps(msg) + "\n").encode())
            except OSError:
                self.sock = None
                GLib.timeout_add_seconds(2, self.connect)

    def send_window(self, w: Wnck.Window, event: str) -> None:
        pid = w.get_pid()
        if pid > 0 and w.get_window_type() == Wnck.WindowType.NORMAL:
            self.send({"cmd": "window", "event": event, "pid": pid, "app": comm(pid)})

    def on_focus(self, screen, _prev) -> None:
        w = screen.get_active_window()
        if w and w.get_pid() > 0:
            self.send({"cmd": "focus", "pid": w.get_pid()})

    def poll_audio(self) -> bool:
        try:
            out = subprocess.run(["pactl", "-f", "json", "list", "sink-inputs"],
                                 capture_output=True, text=True, timeout=2).stdout
            pids = [int(i["properties"]["application.process.id"])
                    for i in json.loads(out or "[]")
                    if "application.process.id" in i.get("properties", {})]
            self.send({"cmd": "audio", "pids": pids})
        except (OSError, ValueError, subprocess.TimeoutExpired):
            pass
        return True                                       # zamanlayıcı devam etsin
    # on_data(): satırları oku, "frozen"/"thawed" olaylarında bildirim göster

def comm(pid: int) -> str:
    try:
        with open(f"/proc/{pid}/comm") as f:
            return f.read().strip()
    except OSError:
        return "?"

if __name__ == "__main__":
    Gtk.init([])
    Agent()
    GLib.MainLoop().run()
```
`pactl -f json` çıktısının biçimi ve `application.process.id` alanının PipeWire altında dolu gelip gelmediği ilk gün doğrulanmalı.

## 9. Arayüz notları
- Tek bir `Gtk.Window` içinde:
  - üstte durum ızgarası: kademe, PSI, RAM, zram oranı;
  - ortada `Gtk.TreeView` uygulama listesi;
  - altta profil seçici (`Gtk.ComboBoxText`) ve politika anahtarı (`Gtk.Switch`).
- `GLib.timeout_add_seconds(1, refresh)`: her saniye yeni bir kısa bağlantı açılır, `status` gönderilir, yanıt 500 ms zaman aşımıyla okunur.
- Servis yanıt vermezse pencerede "servis çalışmıyor" uyarısı gösterilir; pencere asla donmamalı.
- Ayar komutları `ok:false, error:"not authorized"` dönerse kullanıcıya açık bir mesaj gösterilir.

## 10. Kurulum dosyaları

### `systemd/zperfd.service`
```ini
[Unit]
Description=Z-Performans bellek baskısı yöneticisi
Documentation=file:///usr/share/doc/zperf/README.md

[Service]
Type=notify
ExecStart=/usr/sbin/zperfd
ExecStopPost=/usr/libexec/zperf/zperf-thaw-all
Restart=on-failure
RestartSec=1
# Bellek baskısı anında servis öldürülmesin ve belleği geri kazanılmasın:
OOMScoreAdjust=-900
MemoryMin=16M

[Install]
WantedBy=multi-user.target
```

### `scripts/zperf-thaw-all`
```sh
#!/bin/sh
# Servis nasıl durursa dursun donuk uygulama kalmasın (G-08). Hataları yok sayar.
for u in $(systemctl list-units --type=scope --all --no-legend --plain 'zperf-*' | awk '{print $1}'); do
    systemctl thaw "$u" 2>/dev/null || true
done
# TK-02 alternatifi kullanılırsa:
for f in /sys/fs/cgroup/zperf.slice/*/cgroup.freeze; do
    [ -e "$f" ] && echo 0 > "$f"
done
exit 0
```

### `polkit/io.github.zperf.policy`
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <action id="io.github.zperf.manage">
    <description>Z-Performans ayarlarını değiştir</description>
    <message>Z-Performans ayarlarını değiştirmek için yetki gerekiyor</message>
    <defaults>
      <allow_any>auth_admin</allow_any>
      <allow_inactive>no</allow_inactive>
      <allow_active>yes</allow_active>
    </defaults>
  </action>
</policyconfig>
```

### `xdg/zperf-agent.desktop`
```ini
[Desktop Entry]
Type=Application
Name=Z-Performans Ajanı
Exec=/usr/bin/zperf-agent
OnlyShowIn=XFCE;
X-XFCE-Autostart-Override=true
NoDisplay=true
```

### `packaging/DEBIAN/control` (taslak)
```
Package: zperf
Version: 0.1.0
Architecture: amd64
Maintainer: Muhammed Furkan Kaya, Melih Türker
Depends: libsystemd0, libjson-c5, python3, python3-gi, gir1.2-gtk-3.0, gir1.2-wnck-3.0,
 polkitd, pulseaudio-utils
Description: Bellek baskısına uyarlanan bellek yöneticisi
 PSI ile bellek baskısını izler, arka plandaki uygulamaları zram'e sıkıştırır ve dondurur.
```
- `Depends` satırındaki paket adları Pardus'ta `apt-cache policy` ile doğrulanmalı; özellikle `libjson-c5` (sürüm numarası değişebilir).
- `Maintainer` alanı normalde bir e-posta adresi de içerir; okul e-postalarınızı ekleyin.
