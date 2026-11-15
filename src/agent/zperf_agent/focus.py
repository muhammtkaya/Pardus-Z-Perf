# SPDX-License-Identifier: GPL-3.0-or-later
"""Odaktaki pencereyi libwnck ile izleyip servise bildirir.

NOT: Bu modul libwnck (X11) ve GTK gerektirir; yalnizca grafik oturumda
calisir (Pardus 25.0 XFCE/X11). Headless ortamda ice aktarilamaz, bu yuzden
import'lar fonksiyon icinde yapilir ki ipc.py bagimsiz test edilebilsin.
"""
import json
import re

from .notify import send_notification

_SANITIZE = re.compile(r"[^a-z0-9_-]+")


def app_name_from_window(win):
    """Pencereden kararli bir uygulama adi uretir (cgroup adi olarak kullanilir).

    WM_CLASS (class group) tercih edilir; yoksa pencere basligi kirpilir.
    """
    name = ""
    try:
        name = win.get_class_group_name() or ""
    except Exception:
        name = ""
    if not name:
        name = win.get_name() or "app"
    name = _SANITIZE.sub("-", name.strip().lower()).strip("-")
    return name or "app"


class FocusWatcher:
    """Aktif pencere degisimini dinler ve client.send_focus(ad, pid) cagirir."""

    def __init__(self, client):
        self.client = client
        self._last = None
        self._ev_buf = b""

    def _on_active_window_changed(self, screen, _prev):
        win = screen.get_active_window()
        if win is None:
            return
        try:
            pid = win.get_pid()
        except Exception:
            pid = 0
        if not pid:
            return
        name = app_name_from_window(win)
        key = (name, pid)
        if key == self._last:
            return
        self._last = key
        try:
            self.client.send_focus(name, pid)
        except Exception as e:
            # Servis kapali olabilir; ajan cokme, sessizce gec.
            print("odak bildirimi basarisiz:", e)

    def _on_event_readable(self, _source, _cond):
        """Servisin ittigi olay satirlarini (dondu/cozuldu) okuyup bildirir.

        Dondurulen deger True ise izleme surer; False ise GLib kaynagi kaldirir.
        """
        sock = self.client.sock
        if sock is None:
            return False
        try:
            data = sock.recv(4096)
        except (BlockingIOError, InterruptedError):
            return True
        except OSError:
            return False
        if not data:            # servis baglantiyi kapatti
            return False

        self._ev_buf += data
        while b"\n" in self._ev_buf:
            line, self._ev_buf = self._ev_buf.split(b"\n", 1)
            line = line.strip()
            if not line:
                continue
            try:
                msg = json.loads(line.decode("utf-8", "replace"))
            except ValueError:
                continue
            if msg.get("type") != "event":
                continue
            ev = msg.get("event")
            name = msg.get("name", "")
            if ev == "frozen":
                send_notification("Uygulama donduruldu",
                                  f"{name}, bellek baskisi nedeniyle gecici donduruldu.")
            elif ev == "thawed":
                send_notification("Uygulama cozuldu", f"{name} yeniden etkin.")
        return True

    def _setup_event_channel(self):
        """Baglantidaki soketi GLib ana donguye ekleyip olaylari dinler."""
        from gi.repository import GLib
        if self.client.sock is None:
            return
        try:
            self.client.sock.setblocking(False)
            GLib.io_add_watch(self.client.sock.fileno(),
                              GLib.IO_IN | GLib.IO_HUP, self._on_event_readable)
        except Exception as e:  # noqa: BLE001
            print("olay kanali kurulamadi:", e)

    def run(self):
        import gi
        gi.require_version("Gtk", "3.0")
        gi.require_version("Wnck", "3.0")
        from gi.repository import Gtk, Wnck  # noqa: E402

        self._setup_event_channel()

        screen = Wnck.Screen.get_default()
        screen.force_update()
        screen.connect("active-window-changed", self._on_active_window_changed)
        Gtk.main()
