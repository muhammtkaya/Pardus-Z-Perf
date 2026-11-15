# SPDX-License-Identifier: GPL-3.0-or-later
"""Odaktaki pencereyi libwnck ile izleyip servise bildirir.

NOT: Bu modul libwnck (X11) ve GTK gerektirir; yalnizca grafik oturumda
calisir (Pardus 25.0 XFCE/X11). Headless ortamda ice aktarilamaz, bu yuzden
import'lar fonksiyon icinde yapilir ki ipc.py bagimsiz test edilebilsin.
"""
import re

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

    def run(self):
        import gi
        gi.require_version("Gtk", "3.0")
        gi.require_version("Wnck", "3.0")
        from gi.repository import Gtk, Wnck  # noqa: E402

        screen = Wnck.Screen.get_default()
        screen.force_update()
        screen.connect("active-window-changed", self._on_active_window_changed)
        Gtk.main()
