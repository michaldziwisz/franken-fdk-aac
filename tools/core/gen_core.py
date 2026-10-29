#!/usr/bin/env python3
"""Probki do pomiaru rdzenia AAC (pre-echo, dziury w widmie). Tylko stdlib.

attacks48.wav  cicha podsciolka + ostre ataki (kastaniety/klikniecia) w znanych
               miejscach; pozycje atakow zapisuje attacks48.txt (numer probki).
dense48.wav    gesty material szerokopasmowy: akordy, szum, hi-hat, rozne L/R.
               Do 24..50 kbps z pasmem 16..20 kHz wymusza wycinanie pasm.
Uzycie: gen_core.py [katalog]
"""
import math
import os
import random
import struct
import sys
import wave

SR = 48000


def write(path, left, right):
    c = lambda x: max(-32768, min(32767, int(round(x))))
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<hh", c(a), c(b))
                               for a, b in zip(left, right)))


def attacks(out):
    rnd = random.Random(11)
    n = SR * 6
    left = [rnd.uniform(-1, 1) * 30 for _ in range(n)]   # ~ -60 dBFS bed
    right = [rnd.uniform(-1, 1) * 30 for _ in range(n)]
    pos = []
    t = int(0.25 * SR)
    while t < n - SR // 4:
        pos.append(t)
        length = int(0.012 * SR)
        amp = 14000 * rnd.uniform(0.6, 1.0)
        for k in range(length):
            env = math.exp(-k / (0.0025 * SR))
            s = rnd.uniform(-1, 1) * amp * env
            left[t + k] += s
            right[t + k] += s * 0.8 + rnd.uniform(-1, 1) * amp * env * 0.2
        t += int(rnd.uniform(0.28, 0.45) * SR) + rnd.randrange(0, 1024)
    write(os.path.join(out, "attacks48.wav"), left, right)
    with open(os.path.join(out, "attacks48.txt"), "w") as f:
        f.write("\n".join(str(p) for p in pos) + "\n")


def dense(out):
    rnd = random.Random(7)
    n = SR * 6
    chords = [(220, 277.2, 329.6, 440), (196, 246.9, 293.7, 392),
              (174.6, 220, 261.6, 349.2), (164.8, 207.7, 246.9, 329.6)]
    left, right = [0.0] * n, [0.0] * n
    hp_l = hp_r = 0.0
    prev_l = prev_r = 0.0
    for i in range(n):
        t = i / SR
        ch = chords[int(t / 1.5) % 4]
        tone = sum(math.sin(2 * math.pi * f * h * t) / h
                   for f in ch for h in (1, 2, 3, 5, 8))
        nl, nr = rnd.uniform(-1, 1), rnd.uniform(-1, 1)
        # first-difference noise = bright, carries energy up to 20 kHz
        hp_l, prev_l = nl - prev_l, nl
        hp_r, prev_r = nr - prev_r, nr
        hat = 0.0
        if (i % (SR // 8)) < 400:
            hat = math.exp(-(i % (SR // 8)) / 80.0)
        left[i] = 900 * tone + 1800 * hp_l * (0.4 + hat * 3)
        right[i] = 900 * tone * 0.8 + 1800 * hp_r * (0.4 + hat * 3)
    write(os.path.join(out, "dense48.wav"), left, right)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    attacks(out)
    dense(out)
    print("ok", out)
