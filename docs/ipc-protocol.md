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

İstemci kimliği bağlantı kurulurken `SO_PEERCRED` ile okunur (uid/pid loglanır).
Yetkilendirme ileride polkit ile sıkılaştırılacaktır (bkz.
`polkit/org.pardus.zperf.policy`).

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
Bir uygulamayı kurban seçiminden kalıcı olarak çıkarır/geri alır.

```json
{"type":"never_freeze","name":"firefox","on":1}
```

| alan   | tip    | açıklama                        |
|--------|--------|---------------------------------|
| `name` | string | uygulama adı                    |
| `on`   | int    | `1` = asla dondurma, `0` = iptal|

Yanıt yok.

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
{"some":12.34,"full":1.20,"focus":"firefox","profile":"Dengeli","enabled":1}
```

| alan      | tip    | açıklama                                        |
|-----------|--------|-------------------------------------------------|
| `some`    | float  | `/proc/pressure/memory` some avg10 (%), -1 hata |
| `full`    | float  | full avg10 (%), -1 hata                         |
| `focus`   | string | o an odakta sanılan uygulama ("" = yok)         |
| `profile` | string | etkin profilin adı                              |
| `enabled` | int    | yönetim açık mı (`1`/`0`)                        |

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
