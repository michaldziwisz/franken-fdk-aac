#!/usr/bin/env python3
"""PELNY POMIAR NA REALNYM MATERIALE MICHALA.

Trzy pytania, na ktore ten skrypt odpowiada osobno:

 (1) BUG BITSTREAMU - czy realny material wywoluje blad dekodera i czy
     wymuszenie delta-freq dla OPD go usuwa? To wazniejsze od guarda, bo
     dotyczy zgodnosci ze standardem.

 (2) DEGENERACJA - czy w pasmach o skasowanym downmiksie indeksy OPD realnie
     laduja losowo (jump ~2.0 = szum przy 8 stanach), tak jak na syntetykach?

 (3) WARTOSC - czy OPD liczone z resztek jest GORSZE od OPD=0 (czyli czy guard
     ma co uratowac)? Metryka: faza BEZWZGLEDNA kanalu wzgledem oryginalu
     w 60-690 Hz (arg(L)-arg(R) jest strukturalnie slepa - OPD sie skraca).
     Lag wyrownany korelacja przez FFT.
"""
import glob
import os
import re
import subprocess
import sys
import wave
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.expanduser("~/aacfdk_native/front/fdkaac")
SR, LO, HI = 44100, 60.0, 690.0
OUT = os.path.join(HERE, "dumps")


def read_wav(p):
    with wave.open(p, "rb") as w:
        raw = w.readframes(w.getnframes())
        ch = w.getnchannels()
    a = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    return a.reshape(-1, ch)


def encode(wav, tag, extra, env_extra=None):
    m4a = os.path.join(OUT, "r_%s_%s.m4a" % (tag, extra[-1] if extra else "x"))
    env = dict(os.environ, OPD_DUMP="1", OPD_BITS="1")
    if env_extra:
        env.update(env_extra)
    p = subprocess.run([ENC, "-p", "29", "-b", "48000", "--ps-ipd", "1"] +
                       extra + ["-o", m4a, wav], capture_output=True, env=env)
    return m4a, p.stderr.decode(errors="replace")


def dec_errors(m4a):
    d = subprocess.run(["ffmpeg", "-y", "-v", "warning", "-i", m4a,
                        "-f", "null", "-"], capture_output=True)
    log = d.stderr.decode(errors="replace")
    return len(re.findall(r"overflow|illegal", log))


def cyc(a, b, n=8):
    d = np.abs(a - b) % n
    return np.minimum(d, n - d)


def dump_stats(err_text):
    """Zwraca liste (band, ratio_med, jump) z dumpu OPDDUMP."""
    rows = []
    for ln in err_text.splitlines():
        if ln.startswith("OPDDUMP"):
            p = ln.split()
            if len(p) == 12:
                rows.append([int(x) for x in p[1:]])
    if not rows:
        return []
    d = np.array(rows, dtype=np.int64)
    env, band, opd = d[:, 1], d[:, 2], d[:, 10]
    pl, pr, pcr = d[:, 3].astype(float), d[:, 4].astype(float), d[:, 5].astype(float)
    inp = pl + pr
    with np.errstate(divide="ignore", invalid="ignore"):
        ratio = np.where(inp > 0, (pl + pr + 2 * pcr) / inp, np.nan)
    out = []
    for b in sorted(set(band.tolist())):
        m = (band == b) & (env == 0)
        o, r = opd[m], ratio[m]
        if len(o) < 4:
            continue
        out.append((b, float(np.nanmedian(r)), float(cyc(o[1:], o[:-1]).mean())))
    return out


def phase_err(orig, dec, win=8192):
    n = min(len(orig), len(dec))
    f = np.fft.rfftfreq(win, 1.0 / SR)
    sel = (f >= LO) & (f <= HI)
    hw = np.hanning(win)
    acc = []
    for s in range(0, n - win, win):
        O = np.fft.rfft(orig[s:s + win] * hw)
        D = np.fft.rfft(dec[s:s + win] * hw)
        x = np.sum(D[sel] * np.conj(O[sel]))
        if np.abs(x) > 0:
            acc.append(abs(np.degrees(np.angle(x))))
    return float(np.mean(acc)) if acc else float("nan")


def best_lag(a, b, maxlag=20000):
    n = 1 << (len(a) + len(b) - 1).bit_length()
    c = np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(b, n)), n)
    c = np.concatenate([c[-maxlag:], c[:maxlag]])
    return int(np.argmax(np.abs(c)) - maxlag)


def decode_wav(m4a, tag):
    w = os.path.join(OUT, "rdec_%s.wav" % tag)
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", m4a, "-ar", str(SR),
                    "-ac", "2", "-f", "wav", w], check=True)
    return read_wav(w)


if __name__ == "__main__":
    files = sorted(glob.glob(os.path.join(HERE, "real", "M*.wav")))
    print("=" * 84)
    print("(1) BUG BITSTREAMU na REALNYM materiale")
    print("%-5s %-46s %8s %10s %10s" % ("plik", "opis", "opd1", "opd0",
                                        "freq_only"))
    bug = {}
    for f in files:
        tag = os.path.basename(f)[:-4]
        nm = f[:-4] + ".name"
        desc = open(nm).read().strip()[:44] if os.path.exists(nm) else ""
        m1, e1 = encode(f, tag, ["--ps-opd", "1"])
        n1 = dec_errors(m1)
        m0, _ = encode(f, tag, ["--ps-opd", "0"])
        n0 = dec_errors(m0)
        mf, _ = encode(f, tag, ["--ps-opd", "1"], {"OPD_FREQ_ONLY": "1"})
        nf = dec_errors(mf)
        bug[tag] = (n1, n0, nf, e1, desc)
        print("%-5s %-46s %8d %10d %10d" % (tag, desc, n1, n0, nf))

    print()
    print("=" * 84)
    print("(2) DEGENERACJA: pasma o niskim ratio vs nerwowosc indeksu OPD")
    print("    (jump 2.0 = czysty szum przy 8 stanach; <0.5 = stabilny)")
    for f in files:
        tag = os.path.basename(f)[:-4]
        st = dump_stats(bug[tag][3])
        if not st:
            print("%-5s brak dumpu" % tag)
            continue
        low = [(b, r, j) for b, r, j in st if r < 0.1]
        hi = [(b, r, j) for b, r, j in st if r >= 0.1]
        jl = np.mean([j for _, _, j in low]) if low else float("nan")
        jh = np.mean([j for _, _, j in hi]) if hi else float("nan")
        print("%-5s pasm ratio<0.1: %2d (sr.jump %5.2f) | ratio>=0.1: %2d (sr.jump %5.2f)"
              % (tag, len(low), jl, len(hi), jh))
        for b, r, j in sorted(st, key=lambda t: t[1])[:4]:
            print("        band %2d ratio %8.5f jump %5.2f%s"
                  % (b, r, j, "   <-- SZUM" if j >= 1.5 else ""))

    print()
    print("=" * 84)
    print("(3) WARTOSC: czy OPD z resztek jest gorsze niz OPD=0?")
    print("    blad fazy bezwzglednej 60-690 Hz, stopnie (mniej=lepiej)")
    print("%-5s %14s %14s %9s" % ("plik", "OPD=0 (L/P)", "OPD licz (L/P)",
                                  "zysk"))
    for f in files:
        tag = os.path.basename(f)[:-4]
        orig = read_wav(f)
        try:
            m1, _ = encode(f, tag, ["--ps-opd", "1"])
            m0, _ = encode(f, tag, ["--ps-opd", "0"])
            res = {}
            for lbl, m in (("0", m0), ("1", m1)):
                dec = decode_wav(m, tag + lbl)
                lag = best_lag(dec[:, 0], orig[:, 0])
                d = dec[lag:] if lag > 0 else dec
                res[lbl] = [phase_err(orig[:, c], d[:, c]) for c in (0, 1)]
            g = (np.mean(res["0"]) - np.mean(res["1"]))
            print("%-5s %6.1f/%-7.1f %6.1f/%-7.1f %+8.2f"
                  % (tag, res["0"][0], res["0"][1], res["1"][0], res["1"][1], g))
        except Exception as e:  # noqa: BLE001
            print("%-5s BLAD: %s" % (tag, e))
