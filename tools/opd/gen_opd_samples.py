#!/usr/bin/env python3
"""Generatory probek do diagnozy degeneracji OPD przy przeciwfazie.

ZASADA: kazda probka ma ZNANA Z GORY prawidlowa odpowiedz (ground truth), zeby
pomiar mial czym rozstrzygac. IPD/OPD siegaja tylko ~690 Hz (11 pasm
parametrycznych = pierwsze 3 pasma QMF przy 44.1k), wiec CALE zjawisko musi
siedziec w basie/dolnym srodku - inaczej mierzymy szum poza zasiegiem parametru.

Kategorie (numeracja jak w rozmowie z Michalem):
  A eps-sweep    - L=x, R=-x*(1-eps); eps 0.00 -> 0.50. eps=0 to pelna
                   przeciwfaza (downmix ~ 0, OPD nieokreslone), eps=0.5 zdrowe.
  B split-band   - dol (<400 Hz) w przeciwfazie, gora (2-6 kHz) zdrowa.
                   Guard MUSI trafic selektywnie tylko w dolne pasma.
  C quiet        - fade do bardzo cichego; degeneracja NIEZALEZNA od fazy
                   (startowe FIXP_DBL(1) w akumulatorach zaczynaja dominowac).
  D healthy      - KONTRPROBKI: szerokie stereo ze ZGODNYM fazowo basem,
                   pogloz/dekorelacja. Guard nie ma prawa tu wejsc.
  E transition   - przeciwfaza PRZEMIJAJACA (wchodzi i wychodzi) - jedyny
                   material weryfikujacy histereze.
"""
import numpy as np
import wave
import os
import sys

SR = 44100
DUR = 12.0
OUT = os.path.dirname(os.path.abspath(__file__))


def write_wav(path, left, right):
    """16-bit stereo, headroom ~4 dB (nie normalizujemy do zera)."""
    st = np.stack([left, right], axis=1)
    peak = np.max(np.abs(st))
    if peak > 0:
        st = st / peak * 0.63  # ~ -4 dB
    data = (st * 32767.0).astype(np.int16)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())
    return path


def lowband_content(n, seed=1):
    """Tresc skoncentrowana w zasiegu IPD/OPD (60-690 Hz): tony + szum LP."""
    rng = np.random.default_rng(seed)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for f, a in ((70, 1.0), (130, 0.8), (220, 0.6), (330, 0.5), (520, 0.35)):
        x += a * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi))
    # szum przefiltrowany dolnoprzepustowo (kilka splotow biegnaca srednia)
    nz = rng.standard_normal(n)
    k = np.ones(64) / 64
    for _ in range(3):
        nz = np.convolve(nz, k, mode="same")
    x += 6.0 * nz
    # obwiednia z ruchem, zeby nie bylo statycznie
    env = 0.6 + 0.4 * np.sin(2 * np.pi * 0.3 * t)
    return x * env


def hiband_content(n, seed=2):
    rng = np.random.default_rng(seed)
    t = np.arange(n) / SR
    x = np.zeros(n)
    for f, a in ((2200, 0.5), (3700, 0.4), (5300, 0.3)):
        x += a * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi))
    nz = rng.standard_normal(n)
    nz = nz - np.convolve(nz, np.ones(16) / 16, mode="same")  # zgrubny HPF
    return x + 2.0 * nz


def gen_A():
    """eps-sweep: kontrolowane przejscie od pelnej przeciwfazy do zdrowego."""
    n = int(SR * DUR)
    made = []
    for eps in (0.00, 0.02, 0.05, 0.10, 0.20, 0.50):
        x = lowband_content(n, seed=11)
        # dekorelowana domieszka rosnaca z eps - tak "zdrowieje" sygnal
        d = lowband_content(n, seed=99)
        left = x
        right = -(1.0 - eps) * x + eps * d
        p = os.path.join(OUT, "A_antiphase_eps%.2f.wav" % eps)
        write_wav(p, left, right)
        made.append((p, "eps=%.2f" % eps))
    return made


def gen_B():
    """Dol w przeciwfazie, gora zdrowa - test selektywnosci per pasmo."""
    n = int(SR * DUR)
    lo = lowband_content(n, seed=21)
    hi_l = hiband_content(n, seed=22)
    hi_r = hiband_content(n, seed=23)
    left = lo + 0.5 * hi_l
    right = -lo + 0.5 * hi_r
    p = write_wav(os.path.join(OUT, "B_lowband_antiphase.wav"), left, right)
    return [(p, "dol przeciwfaza, gora zdrowa")]


def gen_C():
    """Bardzo cichy fragment - degeneracja z powodu poziomu, nie fazy."""
    n = int(SR * DUR)
    t = np.arange(n) / SR
    x = lowband_content(n, seed=31)
    d = lowband_content(n, seed=32)
    # fade od -6 dB do praktycznie ciszy (-90 dB)
    g = 10 ** (np.linspace(-6, -90, n) / 20.0)
    left = x * g
    right = (0.7 * x + 0.3 * d) * g
    p = write_wav(os.path.join(OUT, "C_quiet_fade.wav"), left, right)
    return [(p, "fade -6 -> -90 dB, faza zdrowa")]


def gen_D():
    """KONTRPROBKI zdrowe: szerokie stereo, zgodny bas, duzo dekorelacji."""
    n = int(SR * DUR)
    made = []
    # D1: szeroki, ale bas ZGODNY fazowo (klasyczny mastering)
    bass = lowband_content(n, seed=41)
    la = lowband_content(n, seed=42)
    ra = lowband_content(n, seed=43)
    left = bass + 0.7 * la + 0.4 * hiband_content(n, seed=44)
    right = bass + 0.7 * ra + 0.4 * hiband_content(n, seed=45)
    made.append((write_wav(os.path.join(OUT, "D1_wide_bass_in_phase.wav"),
                           left, right), "szerokie stereo, bas w fazie"))
    # D2: mocna dekorelacja (poglos) ale bez kasowania sumy
    # UWAGA: np.convolve z IR 0.5 s na 12 s materialu to O(n*m) ~ 1.2e10 -
    # wisi minutami. Splot robimy przez FFT (ta sama pulapka co np.correlate
    # mode='full' w pomiarach panoramy 09.08).
    def fftconv(sig, ir):
        m = len(sig) + len(ir) - 1
        nfft = 1 << (m - 1).bit_length()
        out = np.fft.irfft(np.fft.rfft(sig, nfft) * np.fft.rfft(ir, nfft), nfft)
        return out[: len(sig)]

    rng = np.random.default_rng(46)
    dry = lowband_content(n, seed=47)
    ir_l = rng.standard_normal(SR // 2) * np.exp(-np.arange(SR // 2) / (SR * 0.12))
    ir_r = rng.standard_normal(SR // 2) * np.exp(-np.arange(SR // 2) / (SR * 0.12))
    wl = fftconv(dry, ir_l)
    wr = fftconv(dry, ir_r)
    wl = wl / (np.max(np.abs(wl)) + 1e-12) * np.max(np.abs(dry))
    wr = wr / (np.max(np.abs(wr)) + 1e-12) * np.max(np.abs(dry))
    made.append((write_wav(os.path.join(OUT, "D2_reverb_decorrelated.wav"),
                           dry + 0.5 * wl, dry + 0.5 * wr),
                 "silna dekorelacja, suma niezerowa"))
    return made


def gen_E():
    """Przeciwfaza PRZEMIJAJACA - do weryfikacji histerezy."""
    n = int(SR * DUR)
    t = np.arange(n) / SR
    x = lowband_content(n, seed=51)
    d = lowband_content(n, seed=52)
    # wspolczynnik przechodzi zdrowe -> przeciwfaza -> zdrowe, dwa razy
    # a = +1 zdrowe (R ~ +x), a = -1 przeciwfaza (R ~ -x)
    a = np.cos(2 * np.pi * 0.25 * t)
    left = x
    right = a * x + 0.15 * d
    p = write_wav(os.path.join(OUT, "E_phase_transition.wav"), left, right)
    return [(p, "przeciwfaza wchodzi i wychodzi 0.25 Hz")]


if __name__ == "__main__":
    all_made = []
    for fn in (gen_A, gen_B, gen_C, gen_D, gen_E):
        all_made += fn()
    for p, desc in all_made:
        print("%-40s %8d B  %s" % (os.path.basename(p),
                                   os.path.getsize(p), desc))
    print("razem %d probek w %s" % (len(all_made), OUT))
