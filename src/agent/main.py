#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""zperf-agent giris noktasi: odak degisimlerini zperfd'ye bildirir."""
import sys

from zperf_agent.ipc import ZperfClient, DEFAULT_SOCK
from zperf_agent.focus import FocusWatcher


def main():
    sys.stdout.reconfigure(line_buffering=True)   # journald/terminalde aninda gorunsun
    sock = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCK
    client = ZperfClient(sock)
    try:
        client.connect()
    except OSError as e:
        print(f"zperfd soketine baglanilamadi ({sock}): {e}", file=sys.stderr)
        # Servis henuz baslamamis olabilir; ajan yine de calissin,
        # FocusWatcher periyodik olarak yeniden baglanmayi dener.
    watcher = FocusWatcher(client)
    watcher.run()


if __name__ == "__main__":
    main()
