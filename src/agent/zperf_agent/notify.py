# SPDX-License-Identifier: GPL-3.0-or-later
"""Masaustu bildirimleri (dondu/cozuldu). Grafik oturum gerektirir.

Servis ileride dondurma/cozme olaylarini ajana iletince cagrilacaktir
(su an iskelet; itme kanali sonraki asamada eklenecek).
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
