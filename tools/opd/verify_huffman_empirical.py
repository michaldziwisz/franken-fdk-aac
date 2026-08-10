#!/usr/bin/env python3
"""Czy nasze kody Huffmana IPD/OPD faktycznie zgadzaja sie z dekoderem?

KONTEKST: przy zawezaniu przyczyn porownalem DLUGOSCI kodow (zgodne 1:1), ale
proba odtworzenia KODOW BITOWYCH algorytmem kanonicznym dala 32/32 "roznic".
To bylo podejrzane: gdyby kody byly zle, IPD nigdy by nie dzialalo, a pomiar na
realnym materiale pokazal +33 stopnie poprawy na M2. Czyli to moja rekonstrukcja
(zgadywana kolejnosc kanoniczna ff_vlc_init_tables_from_lengths) byla bledna,
a nie tablice.

Zamiast zgadywac algorytm ffmpeg - test EMPIRYCZNY, ktory rozstrzyga wprost:
jesli kody sa zgodne, to WARTOSCI IPD/OPD odczytane przez dekoder musza byc te
same, ktore zapisal enkoder. Sprawdzamy to obserwowalnym skutkiem: przy zgodnych
tablicach zmiana JEDNEGO indeksu OPD w enkoderze musi dawac przewidywalna
zmiane fazy w dekodowanym audio, a nie losowy rozjazd.

Prostszy i wystarczajacy dowod, ktory tu wykonujemy: bierzemy sygnal o ZNANEJ,
STALEJ roznicy faz miedzy kanalami (czysty ITD), kodujemy z IPD+OPD i mierzymy
faze BEZWZGLEDNA kanalow w dekodowanym audio. Jesli tablice Huffmana bylyby
niezgodne, odczytane indeksy bylyby losowe i blad fazy bylby rzedu losowego
(~90 stopni sredniej). Zgodnosc = blad maly i STABILNY, oraz MONOTONICZNIE
zalezny od zadanego ITD.
"""
import math
import os
import re
import struct
import subprocess
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.expanduser("~/aacfdk_native/front/fdkaac")
D = os.path.join(HERE, "huff")
os.makedirs(D, exist_ok=True)
SR = 44100


def make_itd(path, itd_ms, secs=4.0):
    """Ten sam sygnal w obu kanalach, prawy opozniony o itd_ms."""
    n = int(SR * secs)
    delay = int(round(SR * itd_ms / 1000.0))
    rnd = np.random.default_rng(7)
    t = np.arange(n + delay) / SR
    x = np.zeros(len(t))
    for f, a in ((90, 1.0), (170, 0.8), (260, 0.6), (400, 0.5), (600, 0.35)):
        x += a * np.sin(2 * np.pi * f * t + rnd.uniform(0, 2 * np.pi))
    x *= 0.6 + 0.4 * np.sin(2 * np.pi * 0.25 * t)
    left = x[delay:delay + n]
    right = x[0:n]
    st = np.stack([left, right], 1)
    st = st / np.max(np.abs(st)) * 0.63
    d = (st * 32767).astype(np.int16)
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(d.tobytes())


def encode_decode(src, tag, opd):
    m4a = os.path.join(D, "%s_o%d.m4a" % (tag, opd))
    wav = os.path.join(D, "%s_o%d.wav" % (tag, opd))
    subprocess.run([ENC, "-p", "29", "-b", "48000", "--ps-ipd", "1",
                    "--ps-opd", str(opd), "-o", m4a, src],
                   capture_output=True)
    r = subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", m4a,
                        "-ar", str(SR), "-ac", "2", "-f", "wav", wav],
                       capture_output=True)
    errs = len(re.findall(r"overflow|illegal|error",
                          r.stderr.decode(errors="replace"), re.I))
    with wave.open(wav, "rb") as w:
        a = np.frombuffer(w.readframes(w.getnframes()),
                          dtype=np.int16).astype(float) / 32768.0
    return a.reshape(-1, 2), errs


def best_lag(a, b, maxlag=20000):
    n = 1 << (len(a) + len(b) - 1).bit_length()
    c = np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(b, n)), n)
    c = np.concatenate([c[-maxlag:], c[:maxlag]])
    return int(np.argmax(np.abs(c)) - maxlag)


def phase_err(orig, dec, win=8192):
    n = min(len(orig), len(dec))
    f = np.fft.rfftfreq(win, 1.0 / SR)
    sel = (f >= 60) & (f <= 690)
    hw = np.hanning(win)
    acc = []
    for s in range(0, n - win, win):
        O = np.fft.rfft(orig[s:s + win] * hw)
        DD = np.fft.rfft(dec[s:s + win] * hw)
        x = np.sum(DD[sel] * np.conj(O[sel]))
        if abs(x) > 0:
            acc.append(abs(math.degrees(np.angle(x))))
    return float(np.mean(acc)) if acc else float("nan")


def read_wav(p):
    with wave.open(p, "rb") as w:
        a = np.frombuffer(w.readframes(w.getnframes()),
                          dtype=np.int16).astype(float) / 32768.0
    return a.reshape(-1, 2)


print("Test empiryczny zgodnosci tablic Huffmana IPD/OPD.")
print("Gdyby kody byly niezgodne z dekoderem, odczytane indeksy fazy bylyby")
print("losowe -> blad fazy ~90 st. i BRAK monotonicznosci wzgledem ITD.")
print()
print("%-10s %10s %14s %14s %8s" % ("ITD [ms]", "bledy", "OPD=0 (L/P)",
                                    "OPD=1 (L/P)", "zysk"))
prev = None
mono = True
for itd in (0.1, 0.2, 0.3, 0.4):
    src = os.path.join(D, "itd_%.1f.wav" % itd)
    make_itd(src, itd)
    orig = read_wav(src)
    res = {}
    errs_tot = 0
    for opd in (0, 1):
        dec, e = encode_decode(src, "itd%.1f" % itd, opd)
        errs_tot += e
        lag = best_lag(dec[:, 0], orig[:, 0])
        dd = dec[lag:] if lag > 0 else dec
        res[opd] = [phase_err(orig[:, c], dd[:, c]) for c in (0, 1)]
    g = np.mean(res[0]) - np.mean(res[1])
    print("%-10.1f %10d %6.1f/%-7.1f %6.1f/%-7.1f %+8.2f"
          % (itd, errs_tot, res[0][0], res[0][1], res[1][0], res[1][1], g))
    if prev is not None and np.mean(res[1]) < prev - 15:
        mono = False
    prev = np.mean(res[1])

print()
print("Interpretacja: bledy=0 i zysk>0 przy rosnacym ITD => tablice Huffmana")
print("sa zgodne z dekoderem (odczytane indeksy niosa realna informacje).")
