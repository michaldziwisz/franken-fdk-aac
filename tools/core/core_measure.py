#!/usr/bin/env python3
"""Pomiar rdzenia AAC po dekodowaniu: pre-echo i dziury w widmie.

Uzycie:
  core_measure.py orig.wav dec.wav [--attacks attacks.txt] [--fmin 2000]
                  [--fmax 16000] [--hole-db 15]

Wyrownanie: opoznienie dekodera szukane korelacja (0..6000 probek).
pre-echo  : dla kazdego ataku energia BLEDU (dec - orig) w oknie
            [atak - 1024, atak - 64) wzgledem energii oryginalu w
            [atak, atak + 512). Wynik w dB (nizej = mniej pre-echa), srednia
            i najgorszy przypadek. Bez --attacks pomijany.
dziury    : ramki 1024, pasma 1/3 oktawy w [fmin, fmax]. Dziura = pasmo,
            w ktorym oryginal ma energie powyzej -70 dBFS, a zdekodowany
            sygnal lezy co najmniej --hole-db ponizej. Wynik: procent dziur
            oraz srednia strata energii w dB w pasmach z sygnalem.
nsr       : sredni stosunek energii bledu do sygnalu w tych pasmach (dB,
            nizej = lepiej). Pilnuje, czy mniej dziur nie znaczy po prostu
            wiecej szumu w pozostalych pasmach. Dziura daje nsr ~0 dB.
"""
import argparse
import sys
import wave

import numpy as np


def read(path):
    with wave.open(path, "rb") as w:
        ch, sr, n = w.getnchannels(), w.getframerate(), w.getnframes()
        x = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64)
    return x.reshape(-1, ch) / 32768.0, sr


def align(o, d):
    a = o[:, 0][: 48000 * 2]
    b = d[:, 0][: 48000 * 2 + 6000]
    best, lag = -1e30, 0
    na = np.linalg.norm(a) + 1e-12
    for L in range(0, 6000):
        seg = b[L:L + len(a)]
        if len(seg) < len(a):
            break
        c = float(np.dot(a, seg)) / (na * (np.linalg.norm(seg) + 1e-12))
        if c > best:
            best, lag = c, L
    m = min(len(o), len(d) - lag)
    return o[:m], d[lag:lag + m], lag, best


def preecho(o, d, attacks):
    res = []
    for p in attacks:
        if p - 1024 < 0 or p + 512 > len(o):
            continue
        err = d[p - 1024:p - 64] - o[p - 1024:p - 64]
        att = o[p:p + 512]
        e = float(np.mean(err ** 2)) + 1e-20
        a = float(np.mean(att ** 2)) + 1e-20
        res.append(10 * np.log10(e / a))
    return res


def holes(o, d, sr, fmin, fmax, hole_db):
    n = 1024
    win = np.hanning(n)
    edges = []
    f = fmin
    while f < fmax:
        edges.append((f, min(fmax, f * 2 ** (1 / 3))))
        f *= 2 ** (1 / 3)
    freqs = np.fft.rfftfreq(n, 1 / sr)
    idx = [(np.searchsorted(freqs, lo), np.searchsorted(freqs, hi)) for lo, hi in edges]
    tot = hole = 0
    loss = []
    nsr = []
    for ch in range(o.shape[1]):
        for s in range(0, len(o) - n, n):
            O = np.abs(np.fft.rfft(o[s:s + n, ch] * win)) ** 2
            D = np.abs(np.fft.rfft(d[s:s + n, ch] * win)) ** 2
            for i0, i1 in idx:
                if i1 <= i0:
                    continue
                eo = O[i0:i1].sum() / (n * n / 4)
                if 10 * np.log10(eo + 1e-30) < -70:
                    continue
                ed = D[i0:i1].sum() / (n * n / 4)
                E = np.abs(np.fft.rfft((d[s:s + n, ch] - o[s:s + n, ch]) * win)) ** 2
                ee = E[i0:i1].sum() / (n * n / 4)
                nsr.append(10 * np.log10((ee + 1e-30) / eo))
                dl = 10 * np.log10((ed + 1e-30) / eo)
                tot += 1
                loss.append(min(0.0, dl))
                if dl < -hole_db:
                    hole += 1
    return ((100.0 * hole / tot if tot else 0.0),
            (float(np.mean(loss)) if loss else 0.0),
            (float(np.mean(nsr)) if nsr else 0.0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("orig")
    ap.add_argument("dec")
    ap.add_argument("--attacks")
    ap.add_argument("--fmin", type=float, default=2000)
    ap.add_argument("--fmax", type=float, default=16000)
    ap.add_argument("--hole-db", type=float, default=15)
    a = ap.parse_args()
    o, sr = read(a.orig)
    d, sr2 = read(a.dec)
    if sr != sr2 or o.shape[1] != d.shape[1]:
        sys.exit("rozne parametry plikow")
    o, d, lag, c = align(o, d)
    out = ["lag=%d corr=%.3f" % (lag, c)]
    if a.attacks:
        pos = [int(x) for x in open(a.attacks).read().split()]
        pe = preecho(o, d, pos)
        out.append("preecho_mean=%.2f preecho_worst=%.2f n=%d" %
                   (float(np.mean(pe)), float(np.max(pe)), len(pe)))
    hp, hl, ns = holes(o, d, sr, a.fmin, a.fmax, a.hole_db)
    out.append("holes=%.2f%% loss=%.2fdB nsr=%.2fdB" % (hp, hl, ns))
    print(" ".join(out))


if __name__ == "__main__":
    main()
