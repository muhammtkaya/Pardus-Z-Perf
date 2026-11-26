#!/usr/bin/env python3
"""Analiz: ham ölçümlerden M1–M6 tablosu ve rapor grafikleri (G-43). Sorumlu: Melih.

Kullanım: analyze.py results/raw -o results/
Girdi:    results/raw/<A|B|C|D>_<n>/{probe.csv, collect.csv, load.log, events.log}
Çıktı:    results/olcum_tablosu.md  (her çalıştırmanın değerleri + ortalama ± sd)
          results/sekil4_psi.png    (C ve D, full10/some10 zaman serisi)
          results/sekil5_m1_m3.png  (dört yapılandırma, hata çubuklu)

Tanımlar docs/07 bölüm 5.3'te; hedefler "D'nin C'ye göre" bölüm 5.4'te.
Bağımlılık: python3-matplotlib (Debian paketi).
"""
import argparse
import sys
from pathlib import Path


def m1_stall_seconds(probe_csv: Path) -> float:
    # TODO: 200 ms'yi aşan gecikmelerin toplamı (sn)
    raise NotImplementedError


def m2_heavy_pressure_seconds(collect_csv: Path) -> int:
    # TODO: full10 > 10 olan örnek sayısı (1 sn aralıkla = saniye)
    raise NotImplementedError


def m3_tabs_until_unresponsive(load_log: Path, probe_csv: Path) -> int:
    # TODO: tek seferde 2 sn'yi aşan ilk takılmadan önce açılmış sekme sayısı
    raise NotImplementedError


def m4_oom_kills(load_log: Path) -> int:
    # TODO: load.sh'nin yazdığı OOM sayısı
    raise NotImplementedError


def m5_cost(collect_csv: Path) -> dict:
    # TODO: ortalama cpu_busy_pct, pswpout farkı, zram_orig / zram_compr
    raise NotImplementedError


def m6_thaw_ms(events_log: Path) -> list[float]:
    # TODO: "EVT thawed ... thaw_ms=<n>" satırları (yalnızca D)
    raise NotImplementedError


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("raw", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("results"))
    args = ap.parse_args()
    runs = sorted(p for p in args.raw.iterdir() if p.is_dir())
    print(f"{len(runs)} çalıştırma bulundu: {', '.join(p.name for p in runs)}")
    # TODO: her çalıştırma için M1–M6; yapılandırma başına ortalama ± sd;
    # markdown tablo ve iki grafik. Hedef tutmadıysa tabloya olduğu gibi yazılır.
    return 1


if __name__ == "__main__":
    sys.exit(main())
