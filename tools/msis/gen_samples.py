#!/usr/bin/env python3
"""Probki do testow MS/IS przy short blocks. Tylko stdlib.

short_ms.wav  - silnie skorelowane L~R (MS oplacalne) + gesto rozmieszczone
                transjenty (klik + wybuch szumu co 60 ms) -> duzo ramek
                EIGHT_SHORT z kilkoma grupami okien.
all_short.wav - 48 kHz, atak co 1024 probki (w kazdej ramce) na skorelowanym
                tle z tonami 330/2700/9100 Hz -> ~85% ramek EIGHT_SHORT, MS i IS
                oplacalne, pasmo do ~14 kHz. Do testow przekladu pasm long->short.
is_alt.wav    - kazde pasmo czestotliwosci naprzemiennie TYLKO w L albo TYLKO
                w R (tony w kolejnych pasmach) -> skrajne, przeciwne isScale
                w sasiednich SFB przy wymuszonym IS.
"""
import wave, struct, math, random, sys

SR = 44100


def write(path, L, R):
    c = lambda x: max(-32768, min(32767, int(x)))
    w = wave.open(path, "wb")
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b"".join(struct.pack("<hh", c(l), c(r)) for l, r in zip(L, R)))
    w.close()


def short_ms(path, secs=3.0, seed=11):
    rnd = random.Random(seed)
    n = int(SR * secs)
    L = [0.0] * n; R = [0.0] * n
    period = int(0.060 * SR)
    for i in range(n):
        t = i / SR
        base = 3000 * math.sin(2 * math.pi * 220 * t) + 1500 * math.sin(2 * math.pi * 1330 * t)
        k = i % period
        burst = 0.0
        if k < 400:
            env = math.exp(-k / 90.0)
            burst = 14000 * env * rnd.uniform(-1, 1)
        common = base + burst
        diff = 0.08 * common + 300 * rnd.uniform(-1, 1)
        L[i] = common + diff
        R[i] = common - diff
    write(path, L, R)


def is_alt(path, secs=3.0):
    n = int(SR * secs)
    # srodki pasm ~ co 700 Hz od 2 kHz do 16 kHz, naprzemiennie L / R
    freqs = [2000 + 700 * k for k in range(21)]
    L = [0.0] * n; R = [0.0] * n
    for i in range(n):
        t = i / SR
        sl = sr_ = 0.0
        for k, f in enumerate(freqs):
            v = 1200 * math.sin(2 * math.pi * f * t + k)
            if k % 2 == 0:
                sl += v
            else:
                sr_ += v
        L[i] = sl; R[i] = sr_
    write(path, L, R)


def all_short(path, secs=3.0, seed=5, sr=48000):
    rnd = random.Random(seed)
    n = int(sr * secs)
    c = lambda x: max(-32768, min(32767, int(x)))
    w = wave.open(path, "wb")
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
    out = []
    for i in range(n):
        t = i / sr
        k = i % 1024
        base = (2500 * math.sin(2 * math.pi * 330 * t) + 1200 * math.sin(2 * math.pi * 2700 * t)
                + 700 * math.sin(2 * math.pi * 9100 * t))
        burst = 13000 * math.exp(-k / 60.0) * rnd.uniform(-1, 1) if k < 300 else 0.0
        com = base + burst + 900 * rnd.uniform(-1, 1)
        d = 0.1 * com + 250 * rnd.uniform(-1, 1)
        out.append(struct.pack("<hh", c(com + d), c(com - d)))
    w.writeframes(b"".join(out))
    w.close()


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    short_ms(d + "/short_ms.wav")
    is_alt(d + "/is_alt.wav")
    all_short(d + "/all_short.wav")
    print("ok")
