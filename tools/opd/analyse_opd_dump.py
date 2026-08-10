#!/usr/bin/env python3
"""Analiza dumpu OPDDUMP: czy indeksy OPD degeneruja sie przy skasowanym downmiksie.

Wejscie: stderr enkodera natywnego z OPD_DUMP=1, kolumny
  OPDDUMP frame env band pwrL pwrR pwrCr pwrCi re im ipdIdx opdIdx

Kluczowe metryki PER PASMO:
  ratio  = sumEnergy/inputEnergy = (pwrL+pwrR+2*pwrCr)/(pwrL+pwrR)
           czyli |L+R|^2 / (|L|^2+|R|^2) - jak bardzo downmix sie kasuje.
           1.0 = idealna zgodnosc faz, 0.0 = calkowite skasowanie.
  H      = entropia rozkladu indeksu OPD w czasie (bity, max 3.0 dla 8 stanow).
           Zdrowy sygnal -> niska/skupiona; smieci -> ~3.0 (bialy szum).
  jump   = sredni |delta| indeksu OPD miedzy kolejnymi ramkami (cyklicznie mod 8,
           zakres 0..4). To bezposrednia miara "nerwowosci" i kosztu delta-codingu.

Wynik ma rozstrzygnac DWIE rzeczy:
 1. czy w pasmach o niskim ratio indeksy realnie sa losowe (hipoteza subagenta),
 2. w jakim rzedzie lezy prog ratio, powyzej ktorego OPD jest wiarygodne.
"""
import sys
import numpy as np
from collections import defaultdict


def parse(path):
    rows = []
    with open(path, "r", errors="replace") as f:
        for ln in f:
            if not ln.startswith("OPDDUMP"):
                continue
            p = ln.split()
            if len(p) != 12:
                continue
            rows.append([int(x) for x in p[1:]])
    return np.array(rows, dtype=np.int64) if rows else None


def cyc_delta(a, b, n=8):
    """Najkrotsza odleglosc cykliczna miedzy indeksami."""
    d = np.abs(a - b) % n
    return np.minimum(d, n - d)


def entropy(idx, n=8):
    if len(idx) == 0:
        return 0.0
    c = np.bincount(idx % n, minlength=n).astype(float)
    p = c / c.sum()
    p = p[p > 0]
    return float(-(p * np.log2(p)).sum())


def analyse(path, label):
    d = parse(path)
    if d is None:
        print("%-28s BRAK DANYCH (dump pusty)" % label)
        return None
    frame, env, band = d[:, 0], d[:, 1], d[:, 2]
    pwrL, pwrR, pwrCr = d[:, 3].astype(float), d[:, 4].astype(float), d[:, 5].astype(float)
    opd = d[:, 10]
    ipd = d[:, 9]

    inp = pwrL + pwrR
    summ = pwrL + pwrR + 2.0 * pwrCr
    with np.errstate(divide="ignore", invalid="ignore"):
        ratio = np.where(inp > 0, summ / inp, np.nan)

    print("=" * 78)
    print("%s   ramek=%d  wpisow=%d" % (label, frame.max() + 1, len(d)))
    print("band |  ratio med |  ratio min |   H(OPD) | jump |   H(IPD) | rozklad OPD")
    out = []
    for b in sorted(set(band.tolist())):
        m = band == b
        # bierzemy env 0 dla spojnej serii czasowej
        m0 = m & (env == 0)
        o = opd[m0]
        i2 = ipd[m0]
        r = ratio[m0]
        j = cyc_delta(o[1:], o[:-1]).mean() if len(o) > 1 else 0.0
        h = entropy(o)
        hi = entropy(i2)
        hist = np.bincount(o % 8, minlength=8)
        out.append((b, np.nanmedian(r), np.nanmin(r), h, j, hi))
        print("%4d | %10.4f | %10.4f | %8.3f | %4.2f | %8.3f | %s"
              % (b, np.nanmedian(r), np.nanmin(r), h, j, hi,
                 " ".join("%d" % v for v in hist)))
    return np.array([(o[1], o[3], o[4]) for o in out])


if __name__ == "__main__":
    for path in sys.argv[1:]:
        label = path.split("/")[-1].replace(".err", "")
        analyse(path, label)
