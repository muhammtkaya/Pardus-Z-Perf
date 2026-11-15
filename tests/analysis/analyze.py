#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""analyze.py — olcum ham verisinden M1..M6  olcutlerini hesaplar ve grafik cizer.

Girdi: run.sh'in urettigi bir veya daha cok senaryo klasoru
       (tests/measure/data/<senaryo>-<zaman>/).

Olcutler (bu tur bir bellek-baski yoneticisi icin standart tanimlar; nihai
rapordaki M1..M6 ifadeleriyle gerekirse uyumlanir):

  M1  Etkilesim takilmasi (ms)   : prober.csv stall; ort/p95/p99/max   (DUSUK iyi)
  M2  Takas-giris etkinligi      : pswpin delta (sayfa)                (DUSUK iyi)
  M3  Bellek baskisi (PSI)       : some/full avg10 ort + some>%20 suresi (DUSUK iyi)
  M4  zram verimi                : sikistirma orani + kazanilan bellek  (YUKSEK iyi)
  M5  OOM / kill sayisi          : oom.log satir sayisi                 (0 hedef)
  M6  Servis maliyeti            : zperfd CPU% + RSS zirve              (DUSUK iyi)

Kullanim:
  tests/analysis/analyze.py tests/measure/data/A-* tests/measure/data/C-*
  tests/analysis/analyze.py --out rapor/ tests/measure/data/*/

matplotlib varsa PNG grafikler --out klasorune yazilir; yoksa yalniz tablo basilir.
"""
import argparse
import csv
import glob
import os
import sys

CLK_TCK = os.sysconf("SC_CLK_TCK") if hasattr(os, "sysconf") else 100

# Renk korleri icin ayirt edilebilir palet (Okabe-Ito'dan).
PALETTE = ["#0072B2", "#E69F00", "#009E73", "#D55E00", "#CC79A7", "#56B4E9"]


def _f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


def pctl(vals, p):
    vals = sorted(v for v in vals if v is not None)
    if not vals:
        return 0.0
    idx = p / 100.0 * (len(vals) - 1)
    lo = int(idx)
    if lo >= len(vals) - 1:
        return float(vals[-1])
    frac = idx - lo
    return vals[lo] * (1 - frac) + vals[lo + 1] * frac


def scenario_label(path):
    base = os.path.basename(os.path.normpath(path))
    return base.split("-")[0] or base  # "C-20261116-..." -> "C"


def analyze_dir(path):
    """Bir senaryo klasorunden M1..M6 + zaman serilerini hesaplar."""
    m = {"path": path, "label": scenario_label(path)}

    # M1 — takilma
    prober = read_csv(os.path.join(path, "prober.csv"))
    stalls = [_f(r.get("stall_ms")) for r in prober]
    stalls = [s for s in stalls if s is not None]
    t_stall = [_f(r.get("t_ms")) for r in prober]
    m["stalls"] = stalls
    m["stall_t"] = t_stall
    m["M1_mean"] = sum(stalls) / len(stalls) if stalls else 0.0
    m["M1_p95"] = pctl(stalls, 95)
    m["M1_p99"] = pctl(stalls, 99)
    m["M1_max"] = max(stalls) if stalls else 0.0

    # PSI (M3) zaman serisi
    psi = read_csv(os.path.join(path, "psi.csv"))
    some = [_f(r.get("some_avg10")) for r in psi]
    full = [_f(r.get("full_avg10")) for r in psi]
    psi_t = [_f(r.get("t_ms")) for r in psi]
    some_v = [s for s in some if s is not None]
    full_v = [s for s in full if s is not None]
    m["psi_t"] = psi_t
    m["psi_some"] = some
    m["M3_some_mean"] = sum(some_v) / len(some_v) if some_v else 0.0
    m["M3_full_mean"] = sum(full_v) / len(full_v) if full_v else 0.0
    m["M3_some_over20_pct"] = (100.0 * sum(1 for s in some_v if s > 20) / len(some_v)
                               if some_v else 0.0)

    # M2 — takas giris (pswpin delta)
    mem = read_csv(os.path.join(path, "mem.csv"))
    pin = [_f(r.get("pswpin")) for r in mem]
    pin = [p for p in pin if p is not None]
    m["M2_swapin_pages"] = (pin[-1] - pin[0]) if len(pin) >= 2 else 0.0
    swp = [_f(r.get("swap_used_kb")) for r in mem]
    swp = [s for s in swp if s is not None]
    m["swap_peak_mb"] = max(swp) / 1024.0 if swp else 0.0

    # M4 — zram verimi
    zram = read_csv(os.path.join(path, "zram.csv"))
    ratios, saved = [], []
    for r in zram:
        orig = _f(r.get("orig_bytes"))
        compr = _f(r.get("compr_bytes"))
        used = _f(r.get("mem_used_bytes"))
        if orig and compr and compr > 0:
            ratios.append(orig / compr)
        if orig is not None and used is not None:
            saved.append((orig - used) / 1024.0 / 1024.0)
    m["M4_ratio"] = max(ratios) if ratios else 0.0
    m["M4_saved_mb"] = max(saved) if saved else 0.0

    # M5 — OOM
    oom = os.path.join(path, "oom.log")
    m["M5_oom"] = sum(1 for _ in open(oom)) if os.path.exists(oom) else 0

    # M6 — zperfd maliyeti
    zp = read_csv(os.path.join(path, "zperfd.csv"))
    jif = [_f(r.get("zperfd_cpu_jiffies")) for r in zp]
    jif = [j for j in jif if j is not None]
    rss = [_f(r.get("zperfd_rss_kb")) for r in zp]
    rss = [r for r in rss if r is not None]
    ts = [_f(r.get("t_ms")) for r in zp]
    ts = [t for t in ts if t is not None]
    if len(jif) >= 2 and len(ts) >= 2 and ts[-1] > ts[0]:
        dur_s = (ts[-1] - ts[0]) / 1000.0
        cpu_s = (jif[-1] - jif[0]) / CLK_TCK
        m["M6_cpu_pct"] = 100.0 * cpu_s / dur_s if dur_s > 0 else 0.0
    else:
        m["M6_cpu_pct"] = 0.0
    m["M6_rss_mb"] = (max(rss) / 1024.0) if rss else 0.0
    return m


def print_table(rows):
    cols = [
        ("Senaryo", "label", "{}"),
        ("M1 ort", "M1_mean", "{:.1f}"),
        ("M1 p95", "M1_p95", "{:.1f}"),
        ("M1 p99", "M1_p99", "{:.1f}"),
        ("M2 swapin", "M2_swapin_pages", "{:.0f}"),
        ("M3 some", "M3_some_mean", "{:.1f}"),
        ("M3 >%20", "M3_some_over20_pct", "{:.0f}%"),
        ("M4 oran", "M4_ratio", "{:.2f}x"),
        ("M4 MB", "M4_saved_mb", "{:.0f}"),
        ("M5 OOM", "M5_oom", "{}"),
        ("M6 CPU%", "M6_cpu_pct", "{:.2f}"),
        ("M6 RSS", "M6_rss_mb", "{:.1f}"),
    ]
    header = " | ".join(f"{c[0]:>9}" for c in cols)
    print(header)
    print("-" * len(header))
    for m in rows:
        line = " | ".join(f"{c[2].format(m.get(c[1], 0)):>9}" for c in cols)
        print(line)


def make_charts(rows, out_dir):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as e:  # noqa: BLE001
        print(f"(matplotlib yok, grafik atlandi: {e})")
        return
    os.makedirs(out_dir, exist_ok=True)

    # 1) PSI some zaman serisi (senaryolar ust uste)
    fig, ax = plt.subplots(figsize=(8, 4))
    for i, m in enumerate(rows):
        t = m["psi_t"]; y = m["psi_some"]
        if t and y:
            t0 = next((x for x in t if x is not None), 0)
            xs = [(x - t0) / 1000.0 for x in t if x is not None]
            ys = [v for v in y if v is not None][:len(xs)]
            ax.plot(xs[:len(ys)], ys, label=m["label"], color=PALETTE[i % len(PALETTE)])
    ax.set_xlabel("sure (s)"); ax.set_ylabel("PSI some avg10 (%)")
    ax.set_title("Bellek baskisi (PSI some) — senaryo karsilastirmasi")
    ax.legend(); fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "psi_some.png"), dpi=120)
    plt.close(fig)

    # 2) M1 takilma p95/p99 cubuk
    fig, ax = plt.subplots(figsize=(7, 4))
    labels = [m["label"] for m in rows]
    x = range(len(rows))
    p95 = [m["M1_p95"] for m in rows]
    p99 = [m["M1_p99"] for m in rows]
    w = 0.38
    ax.bar([i - w / 2 for i in x], p95, w, label="p95", color=PALETTE[0])
    ax.bar([i + w / 2 for i in x], p99, w, label="p99", color=PALETTE[1])
    ax.set_xticks(list(x)); ax.set_xticklabels(labels)
    ax.set_ylabel("takilma (ms)"); ax.set_title("M1 — etkilesim takilmasi")
    ax.legend(); fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "m1_stall.png"), dpi=120)
    plt.close(fig)

    # 3) Takilma dagilimi (CDF)
    fig, ax = plt.subplots(figsize=(7, 4))
    for i, m in enumerate(rows):
        s = sorted(m["stalls"])
        if s:
            cdf = [j / len(s) for j in range(1, len(s) + 1)]
            ax.plot(s, cdf, label=m["label"], color=PALETTE[i % len(PALETTE)])
    ax.set_xlabel("takilma (ms)"); ax.set_ylabel("birikimli oran")
    ax.set_title("M1 takilma CDF"); ax.legend(); fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "m1_cdf.png"), dpi=120)
    plt.close(fig)

    print(f"Grafikler yazildi: {out_dir}/psi_some.png, m1_stall.png, m1_cdf.png")


def main():
    ap = argparse.ArgumentParser(description="Olcum verisini M1..M6 olarak analiz et")
    ap.add_argument("dirs", nargs="+", help="senaryo veri klasorleri (glob olabilir)")
    ap.add_argument("--out", default=None, help="grafiklerin yazilacagi klasor")
    args = ap.parse_args()

    paths = []
    for d in args.dirs:
        paths.extend(sorted(glob.glob(d)) if any(c in d for c in "*?[") else [d])
    paths = [p for p in paths if os.path.isdir(p)]
    if not paths:
        print("veri klasoru bulunamadi", file=sys.stderr)
        return 1

    rows = [analyze_dir(p) for p in paths]
    rows.sort(key=lambda m: m["label"])
    print_table(rows)

    if args.out:
        make_charts(rows, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
