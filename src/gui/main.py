#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""zperf-gui giris noktasi."""
import os
import sys

# Gelistirmede ZperfClient'i kardes 'agent' paketinden al; kuruluda ortak
# python modulu olarak bulunur.
try:
    from zperf_agent.ipc import ZperfClient, DEFAULT_SOCK
except ImportError:
    _here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, os.path.join(_here, "..", "agent"))
    from zperf_agent.ipc import ZperfClient, DEFAULT_SOCK

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk  # noqa: E402

from zperf_gui.window import ZperfWindow  # noqa: E402


def main():
    sock = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCK
    client = ZperfClient(sock)
    win = ZperfWindow(client)
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()


if __name__ == "__main__":
    main()
