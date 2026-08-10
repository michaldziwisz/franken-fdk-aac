#!/usr/bin/env python3
"""Kalibracja progu: zaleznosc losowosci OPD od energii downmiksu.

Dla KAZDEJ pary (probka, pasmo, envelope) liczymy:
  ratio = (pwrL+pwrR+2*pwrCr)/(pwrL+pwrR)   -- |L+R|^2 / (|L|^2+|R|^2), 0..2
  jump  = sredni cykliczny |delta| indeksu OPD miedzy ramkami (0..4)
  H     = entropia indeksu OPD (0..3 bity)

Punkt odniesienia: gdyby OPD bylo czystym szumem, dla 8 rownomiernych stanow
  E[jump] = (1+2+3+4+3+2+1)/8 = 2.0     H = 3.0
Zdrowy, wolno dryfujacy parametr ma jump << 1 i H maly.

Binujemy po ratio (logarytmicznie) i patrzymy, GDZIE jump przechodzi od
zdrowego do szumowego. To daje prog z DANYCH.
"""
import glob
import os
import numpy as np

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dumps")
NOISE_JUMP = 2.0
NOISE_H = 3.0


def cyc(a, b, n=8):
    d = np.abs(a - b) % n
    return np.minimum(d, n - d)


def entropy(idx, n=8):
    c = np.bincount(idx % n, minlength=n).astype(float)
    if c.sum() == 0:
        return 0.0
    p = c / c.sum()
    p = p[p > 0]
    return float(-(p * np.log2(p)).sum())


def load(path):
    rows = []
    with open(path, errors="replace") as f:
        for ln in f:
            if ln.startswith("OPDDUMP"):
                p = ln.split()
                if len(p) == 12:
                    rows.append([int(x) for x in p[1:]])
    return np.array(rows, dtype=np.int64) if rows else None


recs = []   # (sample, band, ratio_med, jump, H, nframes)
per_frame = []  # (ratio, jump_to_next)
for path in sorted(glob.glob(os.path.join(D, "*.err"))):
    name = os.path.basename(path)[:-4]
    d = load(path)
    if d is None:
        continue
    env, band, opd = d[:, 1], d[:, 2], d[:, 10]
    pwrL, pwrR, pwrCr = (d[:, 3].astype(float), d[:, 4].astype(float),
                         d[:, 5].astype(float))
    inp = pwrL + pwrR
    summ = pwrL + pwrR + 2.0 * pwrCr
    with np.errstate(divide="ignore", invalid="ignore"):
        ratio = np.where(inp > 0, summ / inp, np.nan)
    for b in sorted(set(band.tolist())):
        m = (band == b) & (env == 0)
        o, r = opd[m], ratio[m]
        if len(o) < 4:
            continue
        j = cyc(o[1:], o[:-1])
        recs.append((name, b, float(np.nanmedian(r)), float(j.mean()),
                     entropy(o), len(o)))
        # per-frame: energia W TEJ ramce vs skok do nastepnej
        rr = np.minimum(r[:-1], r[1:])
        for k in range(len(j)):
            if np.isfinite(rr[k]):
                per_frame.append((rr[k], j[k]))

per_frame = np.array(per_frame)
print("par (ramka,pasmo) w analizie: %d" % len(per_frame))
print()
print("ZALEZNOSC: energia downmiksu -> nerwowosc indeksu OPD")
print("(jump 2.00 i H 3.00 = czysty szum; jump ~0 = stabilny parametr)")
print()
edges = [-1e9, 1e-6, 1e-5, 1e-4, 1e-3, 3e-3, 0.01, 0.03, 0.1, 0.3, 1.0, 3.0]
print("  zakres ratio        |     N | sr.jump | %szumowych(jump>=1.5)")
for i in range(len(edges) - 1):
    lo, hi = edges[i], edges[i + 1]
    m = (per_frame[:, 0] >= lo) & (per_frame[:, 0] < hi)
    if m.sum() == 0:
        continue
    jm = per_frame[m, 1]
    lbl = ("<%.0e" % hi) if i == 0 else ("%.0e .. %.0e" % (lo, hi))
    print("%-20s | %5d | %7.3f | %5.1f%%"
          % (lbl, m.sum(), jm.mean(), 100.0 * (jm >= 1.5).mean()))

print()
print("PASMA WG ratio (kandydaci na guard i na FALSZYWY ALARM):")
print("%-26s %4s %10s %7s %6s" % ("probka", "band", "ratio_med", "jump", "H"))
recs.sort(key=lambda x: x[2])
for name, b, r, j, h, n in recs:
    flag = ""
    if r < 0.01 and j < 0.5:
        flag = "  <-- NISKA ENERGIA ale STABILNE (falszywy alarm progu 0.01)"
    if r < 0.01 and j >= 1.5:
        flag = "  <-- degeneracja (guard uzasadniony)"
    if r >= 0.01 and j >= 1.5:
        flag = "  <-- SZUM przy zdrowej energii (guard NIE pomoze)"
    print("%-26s %4d %10.5f %7.3f %6.3f%s" % (name, b, r, j, h, flag))
