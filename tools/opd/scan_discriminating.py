#!/usr/bin/env python3
"""Skan parametrow generatora STDLIB-owego pod test dyskryminujacy.

check.sh moze uzywac TYLKO biblioteki standardowej (bez numpy), a probka, ktora
lapie regresje (A_antiphase_eps0.50, generowana numpy) nie odtwarza sie 1:1.
Szukamy wiec kombinacji (eps, seed, dlugosc), dla ktorej:
    stara binarka -> bledy dekodera > 0
    nowa binarka  -> 0
Bez tego nowy test w check.sh jest bezwartosciowy (przechodzi tez na zepsutym
kodzie), a tego wlasnie chcemy uniknac - stary test ps-ipd-combo dokladnie tak
przepuscil ten bug.
"""
import math
import os
import random
import re
import struct
import subprocess
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
OLD = os.path.expanduser("~/aacfdk_native/front/fdkaac.prebugfix")
NEW = os.path.expanduser("~/aacfdk_native/front/fdkaac")
D = os.path.join(HERE, "scan")
os.makedirs(D, exist_ok=True)
SR = 44100
PAT = re.compile(r"error|invalid|exceeds|overflow|illegal", re.I)


def lowband(n, seed):
    rnd = random.Random(seed)
    ph = [rnd.uniform(0, 6.283) for _ in range(5)]
    out = []
    st = 0.0
    for i in range(n):
        t = i / SR
        v = (1.0 * math.sin(2 * math.pi * 70 * t + ph[0])
             + 0.8 * math.sin(2 * math.pi * 130 * t + ph[1])
             + 0.6 * math.sin(2 * math.pi * 220 * t + ph[2])
             + 0.5 * math.sin(2 * math.pi * 330 * t + ph[3])
             + 0.35 * math.sin(2 * math.pi * 520 * t + ph[4]))
        st = 0.97 * st + 0.03 * rnd.uniform(-1, 1)
        env = 0.6 + 0.4 * math.sin(2 * math.pi * 0.3 * t)
        out.append((v + 6.0 * st) * env)
    return out


def make(path, eps, seed_x, seed_d, secs):
    n = int(SR * secs)
    x = lowband(n, seed_x)
    d = lowband(n, seed_d)
    mx = max(max(abs(a) for a in x), max(abs(a) for a in d)) or 1.0
    g = 0.63 * 32767.0 / mx
    cl = lambda v: max(-32768, min(32767, int(v)))
    fr = [struct.pack('<hh', cl(x[i] * g),
                      cl((-(1.0 - eps) * x[i] + eps * d[i]) * g))
          for i in range(n)]
    w = wave.open(path, 'wb')
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(fr)); w.close()


def errs(exe, src, out, extra):
    subprocess.run([exe, "-p29", "-b48000", "-f2", "--ps-ipd", "1",
                    "--ps-opd", "1"] + extra + ["-o", out, src],
                   capture_output=True)
    r = subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", out,
                        "-f", "null", "-"], capture_output=True)
    return len(PAT.findall(r.stderr.decode(errors="replace")))


print("szukam kombinacji dyskryminujacej (stara=bledy, nowa=0)")
print("%-32s %8s %6s  %s" % ("wariant", "PRZED", "PO", "ocena"))
found = []
for eps in (0.50, 0.40, 0.30, 0.60):
    for seed_x, seed_d in ((11, 99), (1, 2), (7, 13)):
        for secs in (3, 2):
            tag = "e%02d_s%d_%d_%ds" % (int(eps * 100), seed_x, seed_d, secs)
            wav = os.path.join(D, tag + ".wav")
            make(wav, eps, seed_x, seed_d, secs)
            for extra, lbl in (([], "dflt"),
                               (["--ps-env", "4", "--ps-env-reduce", "0"], "e4")):
                o = errs(OLD, wav, os.path.join(D, "o_%s_%s.aac" % (tag, lbl)), extra)
                n = errs(NEW, wav, os.path.join(D, "n_%s_%s.aac" % (tag, lbl)), extra)
                verdict = ("DYSKRYMINUJE" if o > 0 and n == 0
                           else ("po naprawie nadal bledy" if n > 0 else "-"))
                if o > 0 and n == 0:
                    found.append((tag, lbl, o))
                print("%-32s %8d %6d  %s" % (tag + "/" + lbl, o, n, verdict))
            if found:
                break
        if found:
            break
    if found:
        break

print()
if found:
    print("ZNALEZIONE:", found)
else:
    print("BRAK kombinacji dyskryminujacej w przeskanowanym zakresie")
