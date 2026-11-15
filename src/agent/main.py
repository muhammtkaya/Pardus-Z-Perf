#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""zperf-agent giris noktasi: odak degisimlerini zperfd'ye bildirir."""
import sys

from zperf_agent.ipc import ZperfClient, DEFAULT_SOCK
from zperf_agent.focus import FocusWatcher


def main():
    sock = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCK
    client = ZperfClient(sock)
    try:
        client.connect()
    except OSError as e:
        print(f"zperfd soketine baglanilamadi ({sock}): {e}", file=sys.stderr)
        # Servis henuz baslamamis olabilir; ajan yine de calissin,
        # ilk odakta yeniden baglanma denenir.
    watcher = FocusWatcher(client)
    print("zperf-agent: odak izleme basladi.")
    watcher.run()


if __name__ == "__main__":
    main()
