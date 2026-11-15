<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Paketleme (.deb)

`packaging/debian/` bir Debian (dolayısıyla Pardus) kaynak paketi tanımıdır.
`debhelper` `dh` akışını kullanır; derleme/kurulum üst düzey `Makefile`'a devreder.

## Hızlı kurulum (paketlemesiz)

Paket üretmeden doğrudan sisteme kurmak için:

```bash
make
sudo make install          # /usr/sbin/zperfd, /lib/systemd/system, /etc/zperf/...
sudo systemctl daemon-reload
sudo systemctl enable --now zperfd
```

## .deb üretimi

`debian/` dizini kaynak kökünde beklenir; bu depoda `packaging/debian/` altında
tutulur. Derlemeden önce köke bağlayın (veya kopyalayın):

```bash
ln -sf packaging/debian debian
dpkg-buildpackage -us -uc -b      # imzasız, yalnızca ikili paket
# üst klasörde zperf_0.1.0_<arch>.deb oluşur
```

Gerekli araçlar: `build-essential debhelper dpkg-dev`.

## Paketin içeriği (kurulan yollar)

| Dosya | Hedef |
|-------|-------|
| `zperfd` | `/usr/sbin/zperfd` |
| systemd servisi | `/lib/systemd/system/zperfd.service` |
| polkit politikası | `/usr/share/polkit-1/actions/org.pardus.zperf.policy` |
| profiller | `/etc/zperf/profiles/*.conf` (conffile) |
| ajan + arayüz (python) | `/usr/share/zperf/{agent,gui}` |
| sarmalayıcılar | `/usr/bin/zperf-agent`, `/usr/bin/zperf-gui` |

Servis, `dh_installsystemd` ile kurulumda etkinleştirilip başlatılır.
Ajan ve arayüz kullanıcı oturumunda (XFCE autostart) çalıştırılır; `zperf-agent`
odak bildirimini, `zperf-gui` denetim penceresini açar.
