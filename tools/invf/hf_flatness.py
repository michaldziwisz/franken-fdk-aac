#!/usr/bin/env python3
"""Pomiar zdekodowanego pasma SBR: energia i plaskosc widmowa powyzej fcut.

Plaskosc (SFM, 0..1): bliżej 1 = szumowo/miekko, bliżej 0 = tonalnie/metalicznie.
Uzycie: hf_flatness.py dec.wav [--fcut 9000] [--fhi 20000] [--ref orig.wav]
Okno ustaw na REALNE pasmo SBR z --verbose (core bandwidth .. final BW),
inaczej pusta czesc widma zaniza plaskosc do zera.
Z --ref: dodatkowo roznica energii HF wzgledem oryginalu w dB.
Wymaga numpy.
"""
import sys, wave
import numpy as np


def load(p):
    w = wave.open(p); n = w.getnframes(); ch = w.getnchannels(); sr = w.getframerate()
    x = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64).reshape(-1, ch).mean(axis=1)
    w.close()
    return x, sr


def stats(x, sr, fcut, fhi=20000.0, N=2048):
    win = np.hanning(N)
    e_hf = []; sfm = []
    for s in range(N * 4, len(x) - N, N // 2):
        X = np.abs(np.fft.rfft(x[s:s + N] * win)) ** 2 + 1e-9
        f = np.fft.rfftfreq(N, 1 / sr)
        band = X[(f >= fcut) & (f < min(sr / 2 - 500, fhi))]
        if band.size == 0:
            continue
        e_hf.append(band.sum())
        sfm.append(np.exp(np.mean(np.log(band))) / np.mean(band))
    return 10 * np.log10(np.mean(e_hf) + 1e-9), float(np.median(sfm))


def main():
    a = sys.argv[1:]
    fcut = float(a[a.index("--fcut") + 1]) if "--fcut" in a else 9000.0
    fhi = float(a[a.index("--fhi") + 1]) if "--fhi" in a else 20000.0
    x, sr = load(a[0])
    e, s = stats(x, sr, fcut, fhi)
    out = "hf_dB %.2f  sfm %.3f" % (e, s)
    if "--ref" in a:
        r, rsr = load(a[a.index("--ref") + 1])
        er, sr_ = stats(r, rsr, fcut, fhi)
        out += "  dHF %+.2f dB  sfm_ref %.3f" % (e - er, sr_)
    print(out)


if __name__ == "__main__":
    main()
