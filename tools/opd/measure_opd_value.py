#!/usr/bin/env python3
"""Czy SMIECIOWE OPD realnie SZKODZI dekodowanemu dzwiekowi?

To jest test WARTOSCI, nie tylko diagnostyka. W drzewie mamy juz gotowy
"maksymalny guard": --ps-opd 0 wymusza OPD=0 (zdefiniowana wartosc neutralna).
Wiec porownanie --ps-opd 1 vs --ps-opd 0 na materiale przeciwfazowym mowi
wprost, czy liczenie OPD z resztek jest GORSZE od nieliczenia go wcale.

METRYKA (z lekcji 09.08 - poprzednia metryka byla strukturalnie slepa):
NIE arg(L)-arg(R), bo OPD sie w tej roznicy SKRACA algebraicznie. Mierzymy
faze BEZWZGLEDNA kazdego kanalu wzgledem oryginalu TEGO kanalu:
    X = sum(FFT(dekod) * conj(FFT(orig))) w pasmie 60-690 Hz;  blad = |arg(X)|
Zakres 60-690 Hz, bo tam i TYLKO tam siega IPD/OPD (11 pasm parametrycznych).

Lag kodera/dekodera (2048-3852 probek priming) wyrownujemy korelacja przez FFT
PRZED pomiarem - inaczej mierzymy przesuniecie, nie blad parametru.
"""
import os
import subprocess
import sys
import wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.expanduser("~/aacfdk_native/front/fdkaac")
SR = 44100
LO_HZ, HI_HZ = 60.0, 690.0


def read_wav(path):
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        ch = w.getnchannels()
        raw = w.readframes(n)
    a = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    return a.reshape(-1, ch)


def decode(m4a, out_wav):
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", m4a,
                    "-ar", str(SR), "-ac", "2", "-f", "wav", out_wav],
                   check=True)
    return read_wav(out_wav)


def best_lag(a, b, maxlag=20000):
    """Lag przez FFT (np.correlate mode='full' na 500k probek to O(n^2))."""
    n = 1 << (len(a) + len(b) - 1).bit_length()
    c = np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(b, n)), n)
    c = np.concatenate([c[-maxlag:], c[:maxlag]])
    return int(np.argmax(np.abs(c)) - maxlag)


def phase_err(orig, dec, win=8192):
    """Sredni |arg| iloczynu skrosnego widm w pasmie IPD/OPD, w stopniach."""
    n = min(len(orig), len(dec))
    orig, dec = orig[:n], dec[:n]
    f = np.fft.rfftfreq(win, 1.0 / SR)
    sel = (f >= LO_HZ) & (f <= HI_HZ)
    accs = []
    for s in range(0, n - win, win):
        O = np.fft.rfft(orig[s:s + win] * np.hanning(win))
        D = np.fft.rfft(dec[s:s + win] * np.hanning(win))
        x = np.sum(D[sel] * np.conj(O[sel]))
        if np.abs(x) > 0:
            accs.append(np.abs(np.degrees(np.angle(x))))
    return float(np.mean(accs)) if accs else float("nan")


def run(sample, bitrate="48000"):
    src = os.path.join(HERE, sample + ".wav")
    orig = read_wav(src)
    res = {}
    for tag, extra in (("opd0", ["--ps-opd", "0"]), ("opd1", ["--ps-opd", "1"])):
        m4a = os.path.join(HERE, "dumps", "%s_%s.m4a" % (sample, tag))
        wav = os.path.join(HERE, "dumps", "%s_%s_dec.wav" % (sample, tag))
        cmd = [ENC, "-p", "29", "-b", bitrate, "--ps-ipd", "1"] + extra + \
              ["-o", m4a, src]
        subprocess.run(cmd, check=True, stderr=subprocess.DEVNULL)
        dec = decode(m4a, wav)
        lag = best_lag(dec[:, 0], orig[:, 0])
        d = dec[lag:] if lag > 0 else dec
        errs = [phase_err(orig[:, c], d[:, c]) for c in (0, 1)]
        res[tag] = (errs[0], errs[1], os.path.getsize(m4a))
    return res


if __name__ == "__main__":
    samples = sys.argv[1:] or ["A_antiphase_eps0.00", "A_antiphase_eps0.05",
                               "B_lowband_antiphase", "D1_wide_bass_in_phase"]
    print("Blad fazy BEZWZGLEDNEJ w 60-690 Hz (stopnie, mniej=lepiej)")
    print("%-26s %14s %14s %10s" % ("probka", "OPD=0 (L/P)", "OPD licz (L/P)",
                                    "zysk"))
    for s in samples:
        try:
            r = run(s)
        except Exception as e:  # noqa: BLE001
            print("%-26s BLAD: %s" % (s, e))
            continue
        a0 = (r["opd0"][0] + r["opd0"][1]) / 2
        a1 = (r["opd1"][0] + r["opd1"][1]) / 2
        print("%-26s %6.1f/%-6.1f %6.1f/%-6.1f %+9.2f"
              % (s, r["opd0"][0], r["opd0"][1], r["opd1"][0], r["opd1"][1],
                 a0 - a1))
