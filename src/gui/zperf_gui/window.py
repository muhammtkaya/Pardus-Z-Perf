# SPDX-License-Identifier: GPL-3.0-or-later
"""GTK 3 denetim penceresi.

Gosterir: bellek baskisi (some/full) + canlI mini grafik, odaktaki uygulama.
Yapar: profil secimi (servise gonderir), yonetimi ac/kapat (anahtar),
odaktaki uygulamayi 'asla dondurma'. Durum 1 sn'de bir servisten cekilir.

GTK/PyGObject gerektirir; yalnizca grafik oturumda calisir.
"""
from collections import deque

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib  # noqa: E402

PROFILES = ["Dengeli", "Ofis", "Akilli Tahta"]
HISTORY = 60  # grafikte tutulan ornek sayisi (~1 dk)


class ZperfWindow(Gtk.Window):
    def __init__(self, client):
        super().__init__(title="Pardus Z-Performans")
        self.client = client
        self.focus = ""
        self._sync = False                 # programatik guncellemede sinyali yut
        self._some_hist = deque([0.0] * HISTORY, maxlen=HISTORY)
        self.set_border_width(14)
        self.set_default_size(420, 320)

        grid = Gtk.Grid(column_spacing=12, row_spacing=10)
        self.add(grid)

        def row(r, label, widget):
            lbl = Gtk.Label(label=label, xalign=0)
            grid.attach(lbl, 0, r, 1, 1)
            grid.attach(widget, 1, r, 1, 1)
            return widget

        self.some_lbl = row(0, "Bellek baskisi (some):", Gtk.Label(label="—", xalign=0))
        self.full_lbl = row(1, "Agir baski (full):", Gtk.Label(label="—", xalign=0))
        self.focus_lbl = row(2, "Odaktaki uygulama:", Gtk.Label(label="—", xalign=0))

        # --- canli baski grafigi ---
        self.graph = Gtk.DrawingArea()
        self.graph.set_size_request(-1, 70)
        self.graph.connect("draw", self._on_draw)
        grid.attach(self.graph, 0, 3, 2, 1)

        # --- profil secimi ---
        self.profile = Gtk.ComboBoxText()
        for p in PROFILES:
            self.profile.append_text(p)
        self.profile.set_active(0)
        self.profile.connect("changed", self._on_profile_changed)
        row(4, "Profil:", self.profile)

        # --- yonetimi ac/kapat ---
        self.enable_sw = Gtk.Switch()
        self.enable_sw.set_active(True)
        self.enable_sw.set_halign(Gtk.Align.START)
        self.enable_sw.connect("state-set", self._on_enable_toggled)
        row(5, "Yonetim acik:", self.enable_sw)

        self.nf_btn = Gtk.Button(label="Odaktaki uygulamayi asla dondurma")
        self.nf_btn.connect("clicked", self._on_never_freeze)
        grid.attach(self.nf_btn, 0, 6, 2, 1)

        self.status = Gtk.Label(label="servise baglaniliyor…", xalign=0)
        grid.attach(self.status, 0, 7, 2, 1)

        GLib.timeout_add(1000, self._refresh)

    # ---- cizim ----
    def _on_draw(self, widget, cr):
        w = widget.get_allocated_width()
        h = widget.get_allocated_height()
        # arka plan
        cr.set_source_rgb(0.12, 0.12, 0.14)
        cr.rectangle(0, 0, w, h)
        cr.fill()
        # %25 ve %50 esik cizgileri
        cr.set_line_width(1)
        cr.set_source_rgb(0.3, 0.3, 0.33)
        for frac in (0.25, 0.5, 0.75):
            y = h - frac * h
            cr.move_to(0, y)
            cr.line_to(w, y)
            cr.stroke()
        # baski egrisi (some %, 0..100 -> yukseklik)
        n = len(self._some_hist)
        if n >= 2:
            step = w / (n - 1)
            cr.set_line_width(2)
            cr.set_source_rgb(0.30, 0.69, 0.98)
            for i, v in enumerate(self._some_hist):
                y = h - min(max(v, 0.0), 100.0) / 100.0 * h
                x = i * step
                if i == 0:
                    cr.move_to(x, y)
                else:
                    cr.line_to(x, y)
            cr.stroke()
        return False

    # ---- durum donusu ----
    def _refresh(self):
        try:
            st = self.client.get_status()
            some = float(st.get("some", 0) or 0)
            full = float(st.get("full", 0) or 0)
            self.some_lbl.set_text(f"{some:.1f}%")
            self.full_lbl.set_text(f"{full:.1f}%")
            self.focus = st.get("focus", "") or ""
            self.focus_lbl.set_text(self.focus or "(yok)")
            self.nf_btn.set_sensitive(bool(self.focus))

            self._some_hist.append(max(some, 0.0))
            self.graph.queue_draw()

            self._sync = True                       # servisten gelen degerleri yansit
            try:
                prof = st.get("profile", "")
                if prof and prof in PROFILES:
                    idx = PROFILES.index(prof)
                    if self.profile.get_active() != idx:
                        self.profile.set_active(idx)
                enabled = bool(st.get("enabled", 1))
                if self.enable_sw.get_active() != enabled:
                    self.enable_sw.set_active(enabled)
            finally:
                self._sync = False

            self.status.set_text("servise bagli")
        except Exception:
            self.status.set_text("servise baglanilamadi")
            self.nf_btn.set_sensitive(False)
        return True  # tekrar tetikle

    # ---- kullanici eylemleri ----
    def _on_profile_changed(self, combo):
        if self._sync:
            return
        name = combo.get_active_text()
        if not name:
            return
        try:
            self.client.set_profile(name)
            self.status.set_text(f"Profil: {name}")
        except Exception as e:  # noqa: BLE001
            self.status.set_text(f"profil gonderilemedi: {e}")

    def _on_enable_toggled(self, _switch, state):
        if self._sync:
            return False
        try:
            self.client.set_enabled(state)
            self.status.set_text("Yonetim " + ("acildi" if state else "kapatildi"))
        except Exception as e:  # noqa: BLE001
            self.status.set_text(f"komut gonderilemedi: {e}")
        return False  # varsayilan isleyici anahtari gorsel gunceller

    def _on_never_freeze(self, _btn):
        if self.focus:
            try:
                self.client.set_never_freeze(self.focus, True)
                self.status.set_text(f"'{self.focus}' asla dondurulmayacak")
            except Exception as e:
                self.status.set_text(f"komut gonderilemedi: {e}")
