#!/usr/bin/env python3
"""Probki do pomiarow invf / niskiego bitrate. Tylko stdlib.

mix48.wav  - 10 s stereo 48 kHz: akordy harmoniczne z vibrato (tonalne pasmo
             wysokie), szum rozowawy, ataki perkusyjne, fragment "mowopodobny"
             (szum modulowany formantami). Rusza wszystkie decyzje invf.
voice48.wav- 8 s mono 48 kHz: syntetyczna "mowa" (impulsy glotalne 110-180 Hz
             przez 3 formanty + sybilanty szumowe).
"""
import math, random, struct, wave, sys

SR = 48000


def write(path, chans):
    n = len(chans[0])
    c = lambda x: max(-32768, min(32767, int(x)))
    w = wave.open(path, "wb")
    w.setnchannels(len(chans)); w.setsampwidth(2); w.setframerate(SR)
    fr = bytearray()
    for i in range(n):
        for ch in chans:
            fr += struct.pack("<h", c(ch[i]))
    w.writeframes(bytes(fr)); w.close()


def onepole(x, a):
    y = 0.0; out = []
    for v in x:
        y = a * y + (1 - a) * v; out.append(y)
    return out


def mix(path, secs=10.0, seed=7):
    rnd = random.Random(seed)
    n = int(SR * secs)
    L = [0.0] * n; R = [0.0] * n
    chords = [(220.0, 277.18, 329.63), (196.0, 246.94, 293.66), (174.61, 220.0, 261.63)]
    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    pink = onepole(noise, 0.97)
    for i in range(n):
        t = i / SR
        seg = int(t / 2.5) % 3
        vib = 1.0 + 0.004 * math.sin(2 * math.pi * 5.5 * t)
        s = 0.0
        for k, f0 in enumerate(chords[seg]):
            for h in range(1, 40):
                f = f0 * h * vib
                if f > 20000: break
                s += (900.0 / h ** 0.8) * math.sin(2 * math.pi * f * t + k + h)
        beat = t % 0.5
        perc = 9000 * math.exp(-beat * 40) * noise[i] if beat < 0.12 else 0.0
        hiss = 2500 * pink[i] * 3
        sp = 0.0
        if 6.0 <= t < 8.5:
            sp = 3000 * noise[i] * (0.5 + 0.5 * math.sin(2 * math.pi * 4 * t)) ** 2
        L[i] = 0.55 * s + perc + hiss + sp
        R[i] = 0.50 * s + 0.8 * perc + 0.9 * hiss + sp
    write(path, [L, R])


def voice(path, secs=8.0, seed=3):
    rnd = random.Random(seed)
    n = int(SR * secs)
    out = [0.0] * n
    # resonatory 2. rzedu (formanty) wg prostego IIR
    def reson(x, f, bw):
        r = math.exp(-math.pi * bw / SR); th = 2 * math.pi * f / SR
        a1 = 2 * r * math.cos(th); a2 = -r * r
        y1 = y2 = 0.0; o = []
        for v in x:
            y = v + a1 * y1 + a2 * y2; o.append(y); y2 = y1; y1 = y
        return o
    src = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = i / SR
        f0 = 110 + 70 * (0.5 + 0.5 * math.sin(2 * math.pi * 0.7 * t))
        ph += f0 / SR
        if ph >= 1.0:
            ph -= 1.0; src[i] = 1.0
    voiced = [0.0] * n
    for f, bw, g in ((700, 90, 1.0), (1200, 110, 0.6), (2600, 160, 0.35), (3400, 250, 0.2)):
        r = reson(src, f, bw)
        for i in range(n): voiced[i] += g * r[i]
    m = max(abs(v) for v in voiced) or 1.0
    for i in range(n):
        t = i / SR
        syl = 0.5 + 0.5 * math.sin(2 * math.pi * 3.2 * t)
        sib = 0.0
        if (t % 1.3) > 1.05:
            sib = 5000 * rnd.uniform(-1, 1)
        out[i] = 14000 * voiced[i] / m * syl + sib
    hp = []
    prev = 0.0
    for i, v in enumerate(out):  # proste podbicie gory (sybilanty)
        hp.append(v + 0.0 * prev); prev = v
    write(path, [hp])


def tonal_low(path, secs=6.0, seed=5):
    """Harmoniczne TYLKO ponizej ~5 kHz + szum powyzej 7 kHz: patch SBR
    (kopiowany z tonalnego dolu) jest tonalny, a oryginal w gorze szumowy,
    czyli dokladnie sytuacja, w ktorej estymator wybiera invf HIGH."""
    rnd = random.Random(seed)
    n = int(SR * secs)
    L = [0.0] * n; R = [0.0] * n
    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    # gorny szum: roznica dwoch filtrow jednobiegunowych ~ pasmo > 7 kHz
    lp = onepole(noise, 0.35)
    hi = [noise[i] - lp[i] for i in range(n)]
    for i in range(n):
        t = i / SR
        s = 0.0
        for k, f0 in enumerate((261.63, 329.63, 392.0)):
            for h in range(1, 20):
                f = f0 * h
                if f > 5000: break
                s += (1400.0 / h ** 0.7) * math.sin(2 * math.pi * f * t + k * 1.3 + h)
        L[i] = 0.6 * s + 5000 * hi[i]
        R[i] = 0.55 * s + 4500 * hi[i]
    write(path, [L, R])


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    mix(d + "/mix48.wav")
    voice(d + "/voice48.wav")
    tonal_low(d + "/tonal48.wav")
    print("ok")
