#!/usr/bin/env python3
"""Charakterystyka REALNEGO materialu Michala PRZED kodowaniem.

Cel: sprawdzic czy pliki maja cechy, na ktorych nasz pomiar ma sens, i czy
nie zmarnujemy czasu mierzac cos, czego tam nie ma.

MIERZYMY (per plik, w oknach 1024 probek):
  ratio_lo = |L+R|^2 / (|L|^2+|R|^2) w pasmie 60-690 Hz  <- ZASIEG IPD/OPD
             1.0 = zgodna faza, ~0 = downmix sie kasuje. TO decyduje, czy
             plik w ogole dotyka badanego zjawiska.
  ratio_full = to samo dla calego pasma (dla porownania - pokazuje, czy
             przeciwfaza siedzi w dole czy wyzej, gdzie parametr nie siega)
  corr     = korelacja L/R w dole (klasyczny wskaznik zgodnosci fazy)
  mono_loss_dB = ile energii dolu GINIE przy sumowaniu do mono. To jest
             wielkosc, ktora Michal slyszy jako "znika bas na jednym glosniku".
"""
import glob
import os
import wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 44100
LO, HI = 60.0, 690.0
WIN = 1024


def read_wav(p):
    with wave.open(p, "rb") as w:
        n, ch = w.getnframes(), w.getnchannels()
        raw = w.readframes(n)
    a = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    return a.reshape(-1, ch)


def band_stats(L, R):
    f = np.fft.rfftfreq(WIN, 1.0 / SR)
    sel_lo = (f >= LO) & (f <= HI)
    win = np.hanning(WIN)
    rl, rf, ml = [], [], []
    for s in range(0, len(L) - WIN, WIN):
        FL = np.fft.rfft(L[s:s + WIN] * win)
        FR = np.fft.rfft(R[s:s + WIN] * win)
        for sel, acc in ((sel_lo, rl), (slice(None), rf)):
            eL = np.sum(np.abs(FL[sel]) ** 2)
            eR = np.sum(np.abs(FR[sel]) ** 2)
            eS = np.sum(np.abs(FL[sel] + FR[sel]) ** 2)
            if eL + eR > 1e-12:
                acc.append(eS / (eL + eR))
        eL = np.sum(np.abs(FL[sel_lo]) ** 2)
        eR = np.sum(np.abs(FR[sel_lo]) ** 2)
        eS = np.sum(np.abs(FL[sel_lo] + FR[sel_lo]) ** 2)
        if eL + eR > 1e-12 and eS > 1e-20:
            # strata przy downmiksie wzgledem sumy niekoherentnej
            ml.append(10 * np.log10(eS / (eL + eR)))
    return np.array(rl), np.array(rf), np.array(ml)


print("Charakterystyka materialu w ZASIEGU IPD/OPD (60-690 Hz)")
print("ratio: 1.0=faza zgodna, ~0=downmix sie kasuje. mono_loss: strata basu w mono")
print()
print("%-6s %10s %10s %10s %10s %8s  %s"
      % ("plik", "ratio_lo", "ratio_lo", "ratio_full", "mono_loss", "corr", "opis"))
print("%-6s %10s %10s %10s %10s %8s"
      % ("", "mediana", "5 perc.", "mediana", "dB med", "L/R"))
for p in sorted(glob.glob(os.path.join(HERE, "real", "M*.wav"))):
    tag = os.path.basename(p)[:-4]
    namef = p[:-4] + ".name"
    desc = open(namef).read().strip()[:44] if os.path.exists(namef) else ""
    x = read_wav(p)
    L, R = x[:, 0], x[:, 1]
    rl, rf, ml = band_stats(L, R)
    corr = np.corrcoef(L, R)[0, 1]
    print("%-6s %10.4f %10.4f %10.4f %10.2f %8.3f  %s"
          % (tag, np.median(rl), np.percentile(rl, 5), np.median(rf),
             np.median(ml), corr, desc))

print()
print("INTERPRETACJA PROGOW:")
print("  ratio_lo > 1.5  = bas mocno skorelowany (sumowanie WZMACNIA)")
print("  ratio_lo ~ 1.0  = kanaly zdekorelowane, bez kasowania")
print("  ratio_lo < 0.1  = downmix sie kasuje -> tu zyje badane zjawisko")
