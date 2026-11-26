#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ajan IPC istemcisi + yeniden baglanma testi (root/X11/zperfd GEREKMEZ).

Sahte bir Unix soket sunucusuna karsi dogrular:
  - get_status, arada gelen olay/hata satirlarini atlayip durumu dondurur
  - {"type":"error"} satiri last_error'a kaydedilir
  - servis kopunca ajan bunu algilar, yeniden baglanir, son odagi yeniden
    gonderir ve olay izlemesini yeni sokete tasir
Kullanim:  python3 tests/test_agent_reconnect.py
"""
import json
import os
import socket
import sys
import tempfile
import threading

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "agent"))
from zperf_agent.ipc import ZperfClient  # noqa: E402
from zperf_agent import focus as F  # noqa: E402
from gi.repository import GLib  # noqa: E402

SOCK = os.path.join(tempfile.mkdtemp(prefix="zperf-test-"), "mock.sock")
got = []
class Server:
    def __init__(self):
        if os.path.exists(SOCK): os.unlink(SOCK)
        self.s = socket.socket(socket.AF_UNIX); self.s.bind(SOCK); self.s.listen(4)
        self.conns = []
        threading.Thread(target=self.loop, daemon=True).start()
    def loop(self):
        while True:
            try: c, _ = self.s.accept()
            except OSError: return
            self.conns.append(c)
            threading.Thread(target=self.handle, args=(c,), daemon=True).start()
    def handle(self, c):
        buf = b""
        while True:
            try: d = c.recv(4096)
            except OSError: return
            if not d: return
            buf += d
            while b"\n" in buf:
                l, buf = buf.split(b"\n", 1); m = json.loads(l); got.append(m)
                if m["type"] == "status":
                    # once olay + hata satiri, sonra durum
                    c.sendall(b'{"type":"event","event":"frozen","name":"x"}\n{"type":"error","cmd":"profile","error":"yetki-yok"}\n{"some":1.0,"full":0.0,"focus":"a","profile":"Ofis","enabled":1}\n')
    def stop(self):
        for c in self.conns:
            try: c.shutdown(socket.SHUT_RDWR)
            except OSError: pass
            c.close()
        self.s.close(); os.unlink(SOCK)

ok = True
def check(c, m):
    global ok
    print(("  [GECTI] " if c else "  [KALDI] ") + m); ok &= bool(c)

srv = Server()
cl = ZperfClient(SOCK)
st = cl.get_status()
check(st.get("profile") == "Ofis", "get_status olay/hata satirlarini atladi")
check(cl.last_error and cl.last_error["error"] == "yetki-yok", "hata satiri last_error'a kaydedildi")
cl.close()

# --- ajan yeniden baglanma ---
F.RECONNECT_SEC = 1
F.send_notification = lambda *a: got.append({"type": "notified"}) or True
w = F.FocusWatcher(ZperfClient(SOCK))
w._GLib = GLib
w.client.connect()
w._last = ("firefox", os.getpid())
loop = GLib.MainLoop()
def step1():
    srv.stop()                     # servis coktu
    return False
def step2():
    global srv
    check(not w.client.connected(), "kopma algilandi, baglanti kapatildi")
    got.clear(); srv = Server()    # servis yeniden basladi
    return False
def step3():
    check(w.client.connected(), "ajan yeniden baglandi")
    check(any(m.get("type") == "focus" and m.get("name") == "firefox" for m in got),
          "son odak yeniden gonderildi")
    check(w._watch_id is not None, "olay izlemesi yeni sokete kuruldu")
    loop.quit(); return False
GLib.timeout_add(300, step1); GLib.timeout_add(1200, step2); GLib.timeout_add(3500, step3)
GLib.timeout_add(6000, loop.quit)
loop.run()
srv.stop()
print("PASS" if ok else "FAIL"); sys.exit(0 if ok else 1)
