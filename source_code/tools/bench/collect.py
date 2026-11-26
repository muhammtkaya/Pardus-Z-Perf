#!/usr/bin/env python3
"""Veri toplayıcı (M2, M5, M6). Sorumlu: Melih. Gereksinim: G-41.

Saniyede bir örnek alır ve CSV'ye yazar. Ölçümün başında başlatılır, sonunda
Ctrl+C / SIGTERM ile durdurulur (docs/07 bölüm 5).

Kullanım: collect.py -o run_D_1.csv
Sütunlar: t_wall, some10, full10, some_total_us, full_total_us,
          pswpin, pswpout, cpu_busy_pct, zram_orig, zram_compr, zram_used
"""
import argparse
import csv
import signal
import sys
import time

FIELDS = ["t_wall", "some10", "full10", "some_total_us", "full_total_us",
          "pswpin", "pswpout", "cpu_busy_pct", "zram_orig", "zram_compr", "zram_used"]

stop = False


def on_signal(_sig, _frame) -> None:
    global stop
    stop = True


def read_psi() -> dict:
    # TODO(G-41): /proc/pressure/memory → some10, full10, some_total_us, full_total_us
    return {}


def read_vmstat() -> dict:
    # TODO(G-41): /proc/vmstat → pswpin, pswpout (kümülatif sayfa sayıları)
    return {}


def read_cpu(prev: list[int] | None) -> tuple[float, list[int]]:
    # TODO(G-41): /proc/stat "cpu" satırı; iki örnek arasındaki farktan meşgul yüzde
    # (idle + iowait dışındaki her şey). İlk çağrıda 0 döner.
    return 0.0, prev or []


def read_zram() -> dict:
    # TODO(G-41): /sys/block/zram0/mm_stat ilk üç alan (yoksa boş bırak; A ve B'de zram yok)
    return {}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("-i", "--interval", type=float, default=1.0)
    args = ap.parse_args()
    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    cpu_prev = None
    with open(args.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        while not stop:
            row = {"t_wall": f"{time.time():.3f}"}
            row.update(read_psi())
            row.update(read_vmstat())
            busy, cpu_prev = read_cpu(cpu_prev)
            row["cpu_busy_pct"] = f"{busy:.1f}"
            row.update(read_zram())
            w.writerow(row)
            f.flush()
            time.sleep(args.interval)
    # M6 ve olaylar servis günlüğünden alınır:
    #   journalctl -u zperfd -o cat --since ... | grep '^EVT'
    return 0


if __name__ == "__main__":
    sys.exit(main())
