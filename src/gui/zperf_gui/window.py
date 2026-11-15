# SPDX-License-Identifier: GPL-3.0-or-later
"""GTK 3 denetim penceresi: bellek baskisini gosterir, odaktaki uygulamayi
'asla dondurma' listesine ekler, profil secimi sunar.

GTK/PyGObject gerektirir; yalnizca grafik oturumda calisir.
"""
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib  # noqa: E402

PROFILES = ["Dengeli", "Ofis", "Akilli Tahta"]


class ZperfWindow(Gtk.Window):
    def __init__(self, client):
        super().__init__(title="Pardus Z-Performans")
        self.client = client
        self.focus = ""
        self.set_border_width(14)
        self.set_default_size(380, 240)

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

        self.profile = Gtk.ComboBoxText()
        for p in PROFILES:
            self.profile.append_text(p)
        self.profile.set_active(0)
        row(3, "Profil:", self.profile)

        self.nf_btn = Gtk.Button(label="Odaktaki uygulamayi asla dondurma")
        self.nf_btn.connect("clicked", self._on_never_freeze)
        grid.attach(self.nf_btn, 0, 4, 2, 1)

        self.status = Gtk.Label(label="servise baglaniliyor…", xalign=0)
        grid.attach(self.status, 0, 5, 2, 1)

        GLib.timeout_add(1000, self._refresh)

    def _refresh(self):
        try:
            st = self.client.get_status()
            self.some_lbl.set_text(f"{float(st.get('some', 0)):.1f}%")
            self.full_lbl.set_text(f"{float(st.get('full', 0)):.1f}%")
            self.focus = st.get("focus", "") or ""
            self.focus_lbl.set_text(self.focus or "(yok)")
            self.nf_btn.set_sensitive(bool(self.focus))
            self.status.set_text("servise bagli")
        except Exception:
            self.status.set_text("servise baglanilamadi")
            self.nf_btn.set_sensitive(False)
        return True  # tekrar tetikle

    def _on_never_freeze(self, _btn):
        if self.focus:
            try:
                self.client.set_never_freeze(self.focus, True)
                self.status.set_text(f"'{self.focus}' asla dondurulmayacak")
            except Exception as e:
                self.status.set_text(f"komut gonderilemedi: {e}")
