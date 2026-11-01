#!/usr/bin/env python3
"""Jakosc etapu --resample w izolacji (harness rs_test.c). Uzycie: rs_quality.py <rs_test>

Build harnessu: gcc -O2 -DHAVE_STDINT_H -Isrc-fdkaac/src -Isrc-fdkaac tools/resample/rs_test.c src-fdkaac/src/resample.c -lm -o rs_test
Mierzy: pasmo przenoszenia, aliasy, THD+N 1 kHz, przesuniecie fazy, dlugosc wyjscia."""
import subprocess, sys, numpy as np
RS = sys.argv[1]
def run(x, fin, fout, ch=1, chunk=1024):
    p = subprocess.run([RS, str(fin), str(fout), str(ch), str(chunk)], input=x.astype('<f4').tobytes(),
                       capture_output=True)
    y = np.frombuffer(p.stdout, '<f4').astype(np.float64).reshape(-1, ch)
    return y, p.stderr.decode()
def lvl(y, sr, f, a=0, b=None):
    y = y[a:b]; t = np.arange(len(y))/sr
    w = np.hanning(len(y)); yw = y*w
    c = np.dot(yw, np.cos(2*np.pi*f*t)); s = np.dot(yw, np.sin(2*np.pi*f*t))
    return 2*np.hypot(c, s)/w.sum()
res = {}
for fin, fout in [(48000, 32000), (48000, 24000), (44100, 32000), (48000, 44100), (44100, 22050), (32000, 48000), (44100, 48000), (48000, 16000)]:
    ny = min(fin, fout)/2; N = fin*2
    t = np.arange(N)/fin
    # pasmo przenoszenia
    worst_pb = 0; edge = None
    for f in np.linspace(20, 0.95*ny, 40):
        y, _ = run(0.5*np.sin(2*np.pi*f*t), fin, fout)
        g = 20*np.log10(lvl(y[:, 0], fout, f, 4000, -4000)/0.5)
        worst_pb = max(worst_pb, abs(g))
    # aliasy: tony od nowej Nyquisty do starej (tylko przy downsamplingu)
    worst_al = -999
    if fout < fin:
        for f in np.linspace(ny*1.0, fin/2*0.98, 25):
            y, _ = run(0.5*np.sin(2*np.pi*f*t), fin, fout)
            seg = y[4000:-4000, 0]
            r = 20*np.log10(np.sqrt(np.mean(seg**2))/(0.5/np.sqrt(2)) + 1e-30)
            worst_al = max(worst_al, r)
    # THD+N 1 kHz
    y, _ = run(0.5*np.sin(2*np.pi*1000*t), fin, fout)
    seg = y[4000:-4000, 0]; ts = (np.arange(len(seg))+4000)/fout
    A = np.vstack([np.sin(2*np.pi*1000*ts), np.cos(2*np.pi*1000*ts)]).T
    co, *_ = np.linalg.lstsq(A, seg, rcond=None)
    resid = seg - A@co
    thdn = 20*np.log10(np.sqrt(np.mean(resid**2))/np.sqrt(np.mean((A@co)**2)))
    # opoznienie i faza: idealna sinusoida w tych samych chwilach, bez przesuniecia
    ph = np.degrees(np.arctan2(co[1], co[0]))
    delay_us = -ph/360/1000*1e6
    # dlugosc
    n_in = 100003
    y, err = run(np.zeros(2*n_in), fin, fout, 2, 777)
    exp = -(-n_in*fout//fin)
    print(f"{fin}->{fout}: pasmo do {0.95*ny:.0f} Hz max odch {worst_pb:.4f} dB | aliasy {worst_al:.1f} dB | THD+N 1k {thdn:.1f} dB | przesuniecie {delay_us:+.3f} us | dlugosc {len(y)} (ocz {exp}) {err.strip().splitlines()[-1]} | {err.strip().splitlines()[0].split('(')[1].split(',')[1].strip()}")
