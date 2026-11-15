#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Python ajan istemcisini CALISAN zperfd'ye karsi dogrular.
Kullanim: zperfd calisirken  ->  python3 tests/test_agent_ipc.py [sock]
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "src", "agent"))
from zperf_agent.ipc import ZperfClient  # noqa: E402


def main():
    sock = sys.argv[1] if len(sys.argv) > 1 else "/run/zperf.sock"
    c = ZperfClient(sock)
    c.connect()

    pid = os.getpid()
    print(f"-> focus firefox (pid {pid})")
    c.send_focus("firefox", pid)
    print("-> never_freeze firefox")
    c.set_never_freeze("firefox", True)
    time.sleep(0.2)

    st = c.get_status()
    print("<- durum:", st)
    c.close()

    ok = (st.get("focus") == "firefox")
    print("PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
