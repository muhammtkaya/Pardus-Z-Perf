# SPDX-License-Identifier: GPL-3.0-or-later
"""zperfd ile satir tabanli JSON konusan Unix soket istemcisi.

Protokol (zperfd/ipc.c ile birebir):
  gonder: {"type":"focus","name":<ad>,"pid":<pid>}
  gonder: {"type":"never_freeze","name":<ad>,"on":0|1}
  gonder: {"type":"status"} -> al: {"some":..,"full":..,"focus":".."}

Saf standart kutuphane; GTK/Wnck gerektirmez, bu yuzden bagimsiz test edilebilir.
"""
import json
import socket

DEFAULT_SOCK = "/run/zperf.sock"


class ZperfClient:
    def __init__(self, path=DEFAULT_SOCK):
        self.path = path
        self.sock = None

    def connect(self):
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(self.path)
        self.sock = s

    def _send(self, obj):
        line = (json.dumps(obj, separators=(",", ":")) + "\n").encode()
        if self.sock is None:
            self.connect()
        try:
            self.sock.sendall(line)
        except (BrokenPipeError, OSError):
            # Servis yeniden baslamis olabilir; bir kez yeniden baglan.
            self.connect()
            self.sock.sendall(line)

    def send_focus(self, name, pid):
        self._send({"type": "focus", "name": name, "pid": int(pid)})

    def set_never_freeze(self, name, on):
        self._send({"type": "never_freeze", "name": name, "on": 1 if on else 0})

    def get_status(self, timeout=2.0):
        self._send({"type": "status"})
        self.sock.settimeout(timeout)
        data = self.sock.recv(1024)
        if not data:
            raise ConnectionError("servis yanit vermedi")
        return json.loads(data.decode().strip())

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            finally:
                self.sock = None
