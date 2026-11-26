# SPDX-License-Identifier: GPL-3.0-or-later
"""GTK 3 denetim penceresi (panel).

Gosterir: bellek baskisi (some/full) + canli mini grafik ve yonetilen
uygulamalarin listesi (her biri icin donuk/etkin durumu).
Yapar: profil secimi, yonetimi ac/kapat, ve HER uygulama icin ayri ayri
"Coz" (donmussa hemen coz) ve "Asla dondurma" (sabitle) islemleri.

Tasarim notu: Eylemler "o an odaktaki uygulama"ya gore DEGIL, listedeki
isme gore yapilir. Boylece panele tiklayip odagi GUI'ye almak sorun
olmaz; hedef hep acikca secilen uygulamadir. Durum 1 sn'de bir cekilir.

GTK/PyGObject gerektirir; yalnizca grafik oturumda calisir.
"""
from collections import deque

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib, Pango  # noqa: E402

PROFILES = ["Dengeli", "Ofis", "Akilli Tahta"]
HISTORY = 60  # grafikte tutulan ornek sayisi (~1 dk)


class ZperfWindow(Gtk.Window):
    def __init__(self, client):
        super().__init__(title="Pardus Z-Performans")
        self.client = client
        self._sync = False                 # programatik guncellemede sinyali yut
        self._some_hist = deque([0.0] * HISTORY, maxlen=HISTORY)
        self.app_rows = {}                 # ad -> satir parcacik sozlugu
        self.set_border_width(14)
        self.set_default_size(460, 480)

        grid = Gtk.Grid(column_spacing=12, row_spacing=10)
        self.add(grid)

        def row(r, label, widget):
            lbl = Gtk.Label(label=label, xalign=0)
            grid.attach(lbl, 0, r, 1, 1)
            grid.attach(widget, 1, r, 1, 1)
            return widget

        self.some_lbl = row(0, "Bellek baskisi (some):", Gtk.Label(label="—", xalign=0))
        self.full_lbl = row(1, "Agir baski (full):", Gtk.Label(label="—", xalign=0))

        # --- canli baski grafigi ---
        self.graph = Gtk.DrawingArea()
        self.graph.set_size_request(-1, 70)
        self.graph.connect("draw", self._on_draw)
        grid.attach(self.graph, 0, 2, 2, 1)

        # --- profil secimi ---
        self.profile = Gtk.ComboBoxText()
        for p in PROFILES:
            self.profile.append_text(p)
        self.profile.set_active(0)
        self.profile.connect("changed", self._on_profile_changed)
        row(3, "Profil:", self.profile)

        # --- yonetimi ac/kapat ---
        self.enable_sw = Gtk.Switch()
        self.enable_sw.set_active(True)
        self.enable_sw.set_halign(Gtk.Align.START)
        self.enable_sw.connect("state-set", self._on_enable_toggled)
        row(4, "Yonetim acik:", self.enable_sw)

        # --- yonetilen uygulamalar listesi ---
        head = Gtk.Label(label="Yonetilen uygulamalar", xalign=0)
        head.get_style_context().add_class("dim-label")
        grid.attach(head, 0, 5, 2, 1)

        self.applist = Gtk.ListBox()
        self.applist.set_selection_mode(Gtk.SelectionMode.NONE)
        self._empty_lbl = Gtk.Label(label="(henuz yonetilen uygulama yok)", xalign=0)
        self._empty_lbl.get_style_context().add_class("dim-label")
        self.applist.set_placeholder(self._empty_lbl)
        self._empty_lbl.show()

        scroller = Gtk.ScrolledWindow()
        scroller.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scroller.set_hexpand(True)
        scroller.set_vexpand(True)
        scroller.set_min_content_height(170)
        scroller.add(self.applist)
        grid.attach(scroller, 0, 6, 2, 1)

        self.status = Gtk.Label(label="servise baglaniliyor…", xalign=0)
        self.status.set_line_wrap(True)
        grid.attach(self.status, 0, 7, 2, 1)

        GLib.timeout_add(1000, self._refresh)

    # ---- cizim ----
    def _on_draw(self, widget, cr):
        w = widget.get_allocated_width()
        h = widget.get_allocated_height()
        cr.set_source_rgb(0.12, 0.12, 0.14)
        cr.rectangle(0, 0, w, h)
        cr.fill()
        cr.set_line_width(1)
        cr.set_source_rgb(0.3, 0.3, 0.33)
        for frac in (0.25, 0.5, 0.75):
            y = h - frac * h
            cr.move_to(0, y)
            cr.line_to(w, y)
            cr.stroke()
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

            self._some_hist.append(max(some, 0.0))
            self.graph.queue_draw()

            self._update_apps(st.get("apps") or [])

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

            err = self.client.last_error
            if err:
                self.client.last_error = None
                self.status.set_text(self._error_text(err))
            elif self.status.get_text().startswith("servise baglanilamadi"):
                self.status.set_text("servise bagli")
            elif not self.status.get_text():
                self.status.set_text("servise bagli")
        except Exception:
            self.status.set_text("servise baglanilamadi (zperfd calisiyor mu?)")
        return True  # tekrar tetikle

    # ---- uygulama listesi ----
    def _update_apps(self, apps):
        present = {}
        for a in apps:
            name = a.get("name")
            if name:
                present[name] = a

        for name in list(self.app_rows):           # artik olmayanlari kaldir
            if name not in present:
                self.applist.remove(self.app_rows.pop(name)["row"])

        for name, a in present.items():
            if name not in self.app_rows:
                self._add_app_row(name)
            self._update_app_row(name,
                                 bool(a.get("frozen")),
                                 bool(a.get("never_freeze")),
                                 int(a.get("mem", 0) or 0))

    def _add_app_row(self, name):
        hb = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        hb.set_border_width(4)
        name_lbl = Gtk.Label(label=name, xalign=0)
        name_lbl.set_hexpand(True)
        name_lbl.set_ellipsize(Pango.EllipsizeMode.END)
        state_lbl = Gtk.Label(xalign=1)
        state_lbl.set_width_chars(13)
        thaw_btn = Gtk.Button(label="Çöz")
        thaw_btn.connect("clicked", lambda _b, n=name: self._on_thaw(n))
        nf_chk = Gtk.CheckButton(label="Asla dondurma")
        nf_handler = nf_chk.connect("toggled", lambda _c, n=name: self._on_nf_toggled(n))

        hb.pack_start(name_lbl, True, True, 0)
        hb.pack_start(state_lbl, False, False, 0)
        hb.pack_start(thaw_btn, False, False, 0)
        hb.pack_start(nf_chk, False, False, 0)

        roww = Gtk.ListBoxRow()
        roww.add(hb)
        self.applist.add(roww)
        roww.show_all()
        self.app_rows[name] = {"row": roww, "state": state_lbl,
                               "thaw": thaw_btn, "nf": nf_chk,
                               "nf_handler": nf_handler}

    def _update_app_row(self, name, frozen, never_freeze, mem):
        r = self.app_rows[name]
        mb = mem / (1024 * 1024)
        if frozen:
            r["state"].set_markup(f'<span foreground="#4aa3f0">Donuk</span> · {mb:.0f} MB')
        else:
            r["state"].set_markup(f'<span foreground="#3fb950">Etkin</span> · {mb:.0f} MB')
        r["thaw"].set_sensitive(frozen)
        chk = r["nf"]
        if chk.get_active() != never_freeze:        # kullaniciyla carpismadan yansit
            chk.handler_block(r["nf_handler"])
            chk.set_active(never_freeze)
            chk.handler_unblock(r["nf_handler"])

    @staticmethod
    def _error_text(err):
        why = err.get("error", "")
        cmd = err.get("cmd", "")
        if why == "yetki-yok":
            return f"Yetki yok: '{cmd}' reddedildi (polkit). Etkin oturumda misiniz?"
        return f"Servis '{cmd}' komutunu reddetti: {why}"

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

    def _on_thaw(self, name):
        try:
            self.client.thaw(name)
            self.status.set_text(f"'{name}' cozuluyor…")
        except Exception as e:  # noqa: BLE001
            self.status.set_text(f"komut gonderilemedi: {e}")

    def _on_nf_toggled(self, name):
        r = self.app_rows.get(name)
        if not r:
            return
        on = r["nf"].get_active()
        try:
            self.client.set_never_freeze(name, on)
            self.status.set_text(
                f"'{name}' {'artik asla dondurulmayacak' if on else 'dondurulabilir'}")
        except Exception as e:  # noqa: BLE001
            self.status.set_text(f"komut gonderilemedi: {e}")
