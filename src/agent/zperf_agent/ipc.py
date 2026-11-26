# SPDX-License-Identifier: GPL-3.0-or-later
"""zperfd ile satir tabanli JSON konusan Unix soket istemcisi.

Protokol (zperfd/ipc.c ile birebir; tam liste: docs/ipc-protocol.md):
  gonder: {"type":"focus","name":<ad>,"pid":<pid>}
  gonder: {"type":"never_freeze","name":<ad>,"on":0|1}
  gonder: {"type":"profile","name":<ad>}
  gonder: {"type":"enabled","on":0|1}
  gonder: {"type":"thaw","name":<ad>}
  gonder: {"type":"status"} -> al: {"some":..,"full":..,"focus":..,"profile":..,
           "enabled":..,"apps":[{"name":..,"frozen":0|1,"never_freeze":0|1,"mem":..}]}
  al (itme): {"type":"event","event":"frozen"|"thawed","name":<ad>}
  al (ret):  {"type":"error","cmd":<komut>,"error":<neden>}

Ayni baglantidan hem durum yaniti hem de servisin ittigi olay/hata satirlari
gelebilir; bu yuzden okuma SATIR bazindadir ve "type" alani olan satirlar
durum yaniti sayilmaz.

Saf standart kutuphane; GTK/Wnck gerektirmez, bu yuzden bagimsiz test edilebilir.
"""
import json
import socket
import time

DEFAULT_SOCK = "/run/zperf.sock"


class ZperfClient:
    def __init__(self, path=DEFAULT_SOCK, on_connect=None):
        self.path = path
        self.sock = None
        self.on_connect = on_connect   # her (yeniden) baglantida cagrilir
        self.last_error = None         # servisin son {"type":"error"} satiri
        self._buf = b""

    # ---- baglanti ----
    def connect(self):
        self.close()
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            s.connect(self.path)
        except OSError:
            s.close()
            raise
        self.sock = s
        self._buf = b""
        if self.on_connect:
            self.on_connect(s)

    def connected(self):
        return self.sock is not None

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            finally:
                self.sock = None
                self._buf = b""

    def _send(self, obj):
        line = (json.dumps(obj, separators=(",", ":")) + "\n").encode()
        if self.sock is None:
            self.connect()
        try:
            self.sock.sendall(line)
        except OSError:
            # Servis yeniden baslamis olabilir; bir kez yeniden baglan.
            self.connect()
            self.sock.sendall(line)

    # ---- komutlar ----
    def send_focus(self, name, pid):
        self._send({"type": "focus", "name": name, "pid": int(pid)})

    def set_never_freeze(self, name, on):
        self._send({"type": "never_freeze", "name": name, "on": 1 if on else 0})

    def set_profile(self, name):
        """Calisma zamani profilini degistir (Dengeli/Ofis/Akilli Tahta)."""
        self._send({"type": "profile", "name": name})

    def set_enabled(self, on):
        """Yonetimi ac/kapat (kapatinca donmus uygulamalar cozulur)."""
        self._send({"type": "enabled", "on": 1 if on else 0})

    def thaw(self, name):
        """Adiyla verilen donmus uygulamayi hemen coz (odakta olmasa bile)."""
        self._send({"type": "thaw", "name": name})

    # ---- okuma ----
    def feed(self, data):
        """Gelen ham baytlari tampona ekler, tam satirlari JSON olarak dondurur.
        {"type":"error"} satirlari last_error'a da kaydedilir."""
        self._buf += data
        out = []
        while b"\n" in self._buf:
            raw, self._buf = self._buf.split(b"\n", 1)
            raw = raw.strip()
            if not raw:
                continue
            try:
                msg = json.loads(raw.decode("utf-8", "replace"))
            except ValueError:
                continue
            if isinstance(msg, dict) and msg.get("type") == "error":
                self.last_error = msg
            out.append(msg)
        return out

    def get_status(self, timeout=2.0):
        """Durum ister; aradaki olay/hata satirlarini atlayarak durum yanitini
        dondurur. Baglanti koparsa baglantiyi kapatip hata firlatir (sonraki
        cagri yeniden baglanir)."""
        self._send({"type": "status"})
        deadline = time.monotonic() + timeout
        try:
            while True:
                left = deadline - time.monotonic()
                if left <= 0:
                    raise TimeoutError("servis zamaninda yanit vermedi")
                self.sock.settimeout(left)
                data = self.sock.recv(4096)
                if not data:
                    raise ConnectionError("servis baglantiyi kapatti")
                for msg in self.feed(data):
                    if isinstance(msg, dict) and "type" not in msg:
                        return msg
        except OSError:
            self.close()
            raise
