# SPDX-License-Identifier: GPL-3.0-or-later
"""Odaktaki pencereyi libwnck ile izleyip servise bildirir.

NOT: Bu modul libwnck (X11) ve GTK gerektirir; yalnizca grafik oturumda
calisir (Pardus 25.0 XFCE/X11). Headless ortamda ice aktarilamaz, bu yuzden
import'lar fonksiyon icinde yapilir ki ipc.py bagimsiz test edilebilsin.

Yeniden baglanma: servis kapanir/yeniden baslarsa olay kanali EOF verir;
ajan baglantiyi kapatir ve RECONNECT_SEC aralikla yeniden dener. Baglaninca
olay izlemesi yeni sokete kurulur ve son odak yeniden bildirilir (servis
yeniden basladiginda odak bilgisini kaybetmis olur).
"""
import re

from .notify import send_notification

_SANITIZE = re.compile(r"[^a-z0-9_-]+")
RECONNECT_SEC = 3


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
        self.client.on_connect = self._on_connected
        self._last = None
        self._watch_id = None
        self._retry_id = None
        self._GLib = None

    # ---- baglanti yonetimi ----
    def _on_connected(self, sock):
        """ZperfClient her (yeniden) baglandiginda cagrilir: olay izlemesini
        yeni sokete tasir."""
        if self._GLib is None:
            return                      # run() oncesi; kurulumu run() yapar
        self._remove_watch()
        try:
            sock.setblocking(False)
            self._watch_id = self._GLib.io_add_watch(
                sock.fileno(), self._GLib.IO_IN | self._GLib.IO_HUP | self._GLib.IO_ERR,
                self._on_event_readable)
            print("zperf-agent: servise baglandi.")
        except Exception as e:  # noqa: BLE001
            print("olay kanali kurulamadi:", e)

    def _remove_watch(self):
        if self._watch_id is not None:
            self._GLib.source_remove(self._watch_id)
            self._watch_id = None

    def _lost_connection(self):
        """Olay kanali koptu: kapat ve periyodik yeniden denemeyi baslat."""
        self._watch_id = None           # cagiran False dondurup kaynagi kaldirir
        self.client.close()
        print("zperf-agent: servis baglantisi koptu; yeniden denenecek.")
        self._schedule_retry()

    def _schedule_retry(self):
        if self._retry_id is None:
            self._retry_id = self._GLib.timeout_add_seconds(RECONNECT_SEC, self._try_reconnect)

    def _try_reconnect(self):
        try:
            self.client.connect()       # basarida _on_connected izlemeyi kurar
        except OSError:
            return True                 # servis hala yok; tekrar dene
        self._retry_id = None
        if self._last:                  # servis odak bilgisini kaybetmis olabilir
            try:
                self.client.send_focus(*self._last)
            except OSError:
                pass
        return False

    # ---- odak ----
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
        if not self.client.connected():
            return                      # yeniden baglaninca son odak gonderilir
        try:
            self.client.send_focus(name, pid)
        except OSError as e:
            print("odak bildirimi basarisiz:", e)
            self.client.close()
            self._remove_watch()
            self._schedule_retry()

    # ---- servis olaylari ----
    def _on_event_readable(self, _source, cond):
        """Servisin ittigi olay satirlarini (dondu/cozuldu) okuyup bildirir.

        Dondurulen deger True ise izleme surer; False ise GLib kaynagi kaldirir.
        """
        sock = self.client.sock
        if sock is None:
            self._watch_id = None
            return False
        try:
            data = sock.recv(4096)
        except (BlockingIOError, InterruptedError):
            return True
        except OSError:
            self._lost_connection()
            return False
        if not data:            # servis baglantiyi kapatti
            self._lost_connection()
            return False

        for msg in self.client.feed(data):
            if not isinstance(msg, dict):
                continue
            if msg.get("type") == "error":
                print("servis reddetti:", msg)
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

    def run(self):
        import gi
        gi.require_version("Gtk", "3.0")
        gi.require_version("Wnck", "3.0")
        from gi.repository import GLib, Gtk, Wnck  # noqa: E402

        self._GLib = GLib
        if self.client.connected():
            self._on_connected(self.client.sock)
        else:
            self._schedule_retry()

        screen = Wnck.Screen.get_default()
        screen.force_update()
        screen.connect("active-window-changed", self._on_active_window_changed)
        print("zperf-agent: odak izleme basladi.", flush=True)
        Gtk.main()
