<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# zperfd IPC Protokolü

Servis (`zperfd`) ile istemciler (odak ajanı, GTK arayüzü) arasındaki
haberleşme, bir **Unix alan soketi** üzerinden yürür:

```
/run/zperf.sock   (AF_UNIX, SOCK_STREAM)
```

- Her mesaj **tek satır JSON**'dur; satır `\n` ile biter.
- Sunucu olay güdümlüdür (`epoll`); istemci aynı bağlantıda hem komut
  gönderebilir hem de servis tarafından **itilen olayları** okuyabilir.
- Mesajın tipi `"type"` alanıyla belirlenir. Bilinmeyen tip sessizce yok sayılır.
- Ayrıştırıcı tam bir JSON ayrıştırıcı değildir; yalnızca burada belgelenen
  alanları (`string` ve `int`) tanır. Alanları **basit** tutun (iç içe yok,
  kaçış karakteri yok).

## Yetkilendirme

Soket `0666` izinle oluşturulur; ajan ve arayüz **normal kullanıcı** olarak
bağlanır. Yetki her bağlantı için `SO_PEERCRED` (uid/pid) ile denetlenir:

| komut | kim gönderebilir |
|---|---|
| `status` | herkes |
| `focus` | root; normal kullanıcı yalnızca **kendi** süreçlerinin PID'iyle (`/proc/<pid>/status` Uid). PID ≤ 1 her zaman reddedilir. Aksi hâlde bir kullanıcı başkasının sürecini dondurtabilirdi. |
| `never_freeze`, `profile`, `enabled`, `thaw` | root; normal kullanıcı için polkit `org.pardus.zperf.manage` (`pkcheck --process pid,başlangıç,uid`). Varsayılan: etkin yerel oturuma parolasız izin, diğerlerine ret. Sonuç bağlantı başına önbelleklenir. |

`name` alanları cgroup dizin adına dönüştüğü için yalnızca `[A-Za-z0-9._-]`
kabul edilir ve `.` ile başlayamaz (profil adlarında boşluk da serbest).
Reddedilen komut için servis bir `error` satırı döndürür (aşağıda).

---

## İstemci → Servis (komutlar)

### `focus` — odak bildirimi
Odaktaki pencere değiştiğinde ajan gönderir. Servis uygulamayı yönetime alır
(cgroup'a taşır), donmuşsa çözer ve LRU zamanını günceller.

```json
{"type":"focus","name":"firefox","pid":1234}
```

| alan   | tip    | açıklama                                             |
|--------|--------|------------------------------------------------------|
| `name` | string | kararlı uygulama adı (WM_CLASS'tan; cgroup adı olur) |
| `pid`  | int    | pencerenin kök süreç PID'i                           |

Yanıt yok.

### `never_freeze` — "asla dondurma" bayrağı
Bir uygulamayı kurban seçiminden çıkarır (`on:1`) veya geri alır (`on:0`).
`on:0` yapılınca uygulama **anında** yeniden kurban adayı olur (eski koruma
hatırlanmaz). `on:1` yapıldığında uygulama **o an donuksa ayrıca hemen çözülür**
(çünkü "asla dondurma" denen bir şeyin donuk kalması tutarsız olur).

```json
{"type":"never_freeze","name":"firefox","on":1}
```

| alan   | tip    | açıklama                        |
|--------|--------|---------------------------------|
| `name` | string | uygulama adı                    |
| `on`   | int    | `1` = asla dondurma, `0` = iptal|

Yanıt yok (`on:1` donuk uygulamayı çözerse tüm istemcilere `thawed` olayı itilir).

### `thaw` — bir uygulamayı elle çöz
Adıyla verilen **donmuş** uygulamayı hemen çözer; uygulama odakta olmasa bile
çalışır (panelin "Çöz" butonu bunu gönderir). Donmuş değilse/yoksa sessizce yok
sayılır.

```json
{"type":"thaw","name":"libreoffice"}
```

| alan   | tip    | açıklama     |
|--------|--------|--------------|
| `name` | string | uygulama adı |

Yanıt yok (çözülürse tüm istemcilere `thawed` olayı itilir).

### `profile` — profil seçimi
Çalışma zamanı profilini değiştirir (eşikler + reclaim adımları + çözme süresi).
Geçerli adlar: `Dengeli`, `Ofis`, `Akilli Tahta` (büyük/küçük harf ve boşluk
önemsiz; slug olarak eşlenir). Bilinmeyen ad yok sayılır (logda uyarı).

```json
{"type":"profile","name":"Ofis"}
```

Yanıt yok. Ayrıntı: `config/profiles/<slug>.conf` ve `src/zperfd/profile.c`.

### `enabled` — yönetimi aç/kapat
Yönetimi kapatınca servis hiçbir reclaim/freeze yapmaz ve **donmuş tüm
uygulamaları çözer**. Açınca normal çalışmaya döner.

```json
{"type":"enabled","on":0}
```

| alan | tip | açıklama                      |
|------|-----|-------------------------------|
| `on` | int | `1` = açık, `0` = kapalı       |

Yanıt yok.

### `status` — durum sorgusu
Tek satır JSON **yanıt** alır (aşağıda).

```json
{"type":"status"}
```

---

## Servis → İstemci

### `status` yanıtı
`status` komutuna karşılık, servis tek satır JSON döndürür:

```json
{"some":12.34,"full":1.20,"focus":"firefox","profile":"Dengeli","enabled":1,
 "apps":[{"name":"firefox","frozen":0,"never_freeze":1,"mem":734003200},
         {"name":"libreoffice","frozen":1,"never_freeze":0,"mem":12582912}]}
```

| alan      | tip    | açıklama                                        |
|-----------|--------|-------------------------------------------------|
| `some`    | float  | `/proc/pressure/memory` some avg10 (%), -1 hata |
| `full`    | float  | full avg10 (%), -1 hata                         |
| `focus`   | string | o an odakta sanılan uygulama ("" = yok)         |
| `profile` | string | etkin profilin adı                              |
| `enabled` | int    | yönetim açık mı (`1`/`0`)                        |
| `apps`    | dizi   | yönetilen uygulamalar (panel listesi; aşağıda)  |

`apps` her öğesi bir yönetilen uygulamadır (kendi arayüzümüz `zperf-*` hariç):

| alan           | tip    | açıklama                                  |
|----------------|--------|-------------------------------------------|
| `name`         | string | uygulama adı (cgroup adı)                 |
| `frozen`       | int    | şu an donmuş mu (`1`/`0`)                 |
| `never_freeze` | int    | "asla dondurma" sabiti açık mı (`1`/`0`)  |
| `mem`          | int    | `memory.current` (bayt), bilinmezse `0`   |

> Not: Tampon dolarsa listedeki fazla uygulamalar atlanır ama JSON geçerli
> kapanır. `name` her zaman `[A-Za-z0-9._-]` olduğundan JSON'a güvenle gömülür.

### `event` itme (servis başlatır)
Servis, bir uygulamayı dondurduğunda/çözdüğünde bağlı **tüm** istemcilere
kendiliğinden yollar (istek gerekmez). Ajan bunu masaüstü bildirimine çevirir.

```json
{"type":"event","event":"frozen","name":"libreoffice"}
{"type":"event","event":"thawed","name":"libreoffice"}
```

| alan    | tip    | değer                                   |
|---------|--------|-----------------------------------------|
| `event` | string | `frozen` (donduruldu) / `thawed` (çözüldü) |
| `name`  | string | ilgili uygulama adı                     |

### `error` — komut reddedildi
Yetki ya da doğrulama başarısız olduğunda yalnızca komutu gönderen istemciye:

```json
{"type":"error","cmd":"profile","error":"yetki-yok"}
```

| `error` | anlamı |
|---|---|
| `yetki-yok` | polkit izni yok (etkin oturum değil vb.) |
| `pid-yetkisi-yok` | `focus` PID'i istemcinin kendi süreci değil |
| `gecersiz-ad` | `name` izin verilmeyen karakter içeriyor |

> Not: `status` yanıtı ile `event`/`error` satırları **aynı bağlantıdan** gelir.
> Yanıtı okuyan istemci satır satır okumalı ve `"type"` alanı olan satırları
> durum yanıtı saymamalıdır (`ZperfClient.get_status` böyle yapar).

> Not: İtme kanalını kullanmak isteyen istemci, soketi okunabilir olduğunda
> satır satır okumalıdır (ajan `GLib.io_add_watch` ile yapar). Yalnızca komut
> gönderen bir istemci (örn. yalın test betiği) olayları yok sayabilir.

---

## Örnek oturum

```
C→S  {"type":"profile","name":"Akilli Tahta"}
C→S  {"type":"focus","name":"firefox","pid":1990}
C→S  {"type":"status"}
S→C  {"some":30.1,"full":4.0,"focus":"firefox","profile":"Akilli Tahta","enabled":1}
     ... bellek baskısı artar, servis bir kurbanı dondurur ...
S→C  {"type":"event","event":"frozen","name":"libreoffice"}
```

## Referans gerçekleme
- Sunucu: `src/zperfd/ipc.c` (ayrıştırma, `dispatch_line`, `ipc_broadcast`)
- İstemci (Python, saf stdlib): `src/agent/zperf_agent/ipc.py`
