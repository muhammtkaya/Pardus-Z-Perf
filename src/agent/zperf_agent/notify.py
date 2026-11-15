# SPDX-License-Identifier: GPL-3.0-or-later
"""Masaustu bildirimleri (dondu/cozuldu). Grafik oturum gerektirir.

Servis, dondurma/cozme olaylarini /run/zperf.sock uzerinden ajana iter
(bkz. docs/ipc-protocol.md "event"); FocusWatcher bu olaylari alip burayi
cagirir. libnotify (gi Notify) yoksa sessizce False doner.
"""


def send_notification(summary, body=""):
    try:
        import gi
        gi.require_version("Notify", "0.7")
        from gi.repository import Notify
        if not Notify.is_initted():
            Notify.init("zperf-agent")
        Notify.Notification.new(summary, body, "dialog-information").show()
        return True
    except Exception as e:
        print("bildirim gosterilemedi:", e)
        return False
