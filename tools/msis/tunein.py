#!/usr/bin/env python3
"""Pomiar "wejscia w srodek strumienia": po ilu sekundach od dostrojenia
dekoder zaczyna grac pasmo SBR (gora widma), czyli kiedy dostal sbr_header.

To jest kryterium widziane przez sluchacza Icecast/Shoutcast/HLS, mierzone
po stronie DEKODERA (ffmpeg), nie z deklaracji enkodera.

Metoda: pelne dekodowanie = referencja. Dla wielu punktow startu k strumien
ADTS jest ucinany od ramki k i dekodowany od nowa. Dla kazdej ramki liczymy
energie powyzej --fcut (Hz) w ucietym i w referencji (wyrownane po pozycji).
SBR "gra", gdy energia gornego pasma >= 30% referencji przez 3 ramki z rzedu.
Opoznienie = czas od startu do tego momentu.

Uzycie: tunein.py plik.aac|plik.loas --fcut 12000 [--step 7] [--dec ffmpeg|faad|fdk]
  --dec fdk  = dekoder Fraunhofera (tools/msis/fdkdec.c, sciezka --fdkdec);
               jedyny z trzech, ktory gra AAC-ELD z LD-SBR (LOAS).
Wymaga numpy i ffmpeg (opcjonalnie faad / fdkdec).
"""
import sys, os, subprocess, argparse, wave
import numpy as np


def frames(data):
    offs = []; i = 0
    loas = ((data[0] << 3) | (data[1] >> 5)) == 0x2B7
    while i + 7 <= len(data):
        if loas:
            flen = 3 + (((data[i + 1] & 0x1F) << 8) | data[i + 2])
        else:
            flen = ((data[i + 3] & 3) << 11) | (data[i + 4] << 3) | (data[i + 5] >> 5)
        offs.append(i); i += flen
    return offs


FDKDEC = os.path.expanduser("~/aacfdk_native/fdkdec")
FL = 2048


def decode(buf, tmp, dec):
    a = tmp + ".aac"; w = tmp + ".wav"
    open(a, "wb").write(buf)
    if dec == "fdk":
        subprocess.run([FDKDEC, a, w, str(FL)], capture_output=True)
    elif dec == "faad":
        subprocess.run(["faad", "-q", "-o", w, a], capture_output=True)
    else:
        subprocess.run(["ffmpeg", "-v", "quiet", "-y", "-i", a, "-ac", "1",
                        "-c:a", "pcm_s16le", w], capture_output=True)
    r = wave.open(w)
    sr = r.getframerate(); ch = r.getnchannels()
    x = np.frombuffer(r.readframes(r.getnframes()), dtype=np.int16).astype(np.float64)
    r.close()
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    os.remove(a); os.remove(w)
    return x, sr


def hf_per_frame(x, sr, fl, fcut):
    n = len(x) // fl
    if n == 0:
        return np.zeros(0)
    seg = x[:n * fl].reshape(n, fl) * np.hanning(fl)
    sp = np.abs(np.fft.rfft(seg, axis=1)) ** 2
    k0 = int(fcut * fl / sr)
    return sp[:, k0:].sum(axis=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("f")
    ap.add_argument("--fcut", type=float, default=12000.0)
    ap.add_argument("--step", type=int, default=7)
    ap.add_argument("--fl", type=int, default=2048, help="probek wyjsciowych na ramke")
    ap.add_argument("--dec", default="ffmpeg")
    ap.add_argument("--maxstarts", type=int, default=60)
    ap.add_argument("--fdkdec", default=None)
    a = ap.parse_args()
    global FDKDEC, FL
    if a.fdkdec:
        FDKDEC = a.fdkdec
    FL = a.fl
    data = open(a.f, "rb").read()
    offs = frames(data)
    tmp = a.f + ".tunein"
    ref, sr = decode(data, tmp, a.dec)
    href = hf_per_frame(ref, sr, a.fl, a.fcut)
    thr_sig = np.percentile(href, 60) * 0.05
    tail = max(40, int(0.3 * len(offs)))
    starts = list(range(1, len(offs) - tail, a.step))[:a.maxstarts]
    lat = []; never = 0
    for k in starts:
        x, _ = decode(data[offs[k]:], tmp, a.dec)
        h = hf_per_frame(x, sr, a.fl, a.fcut)
        r = href[k:k + len(h)]
        m = min(len(h), len(r))
        ok = (r[:m] > thr_sig) & (h[:m] >= 0.3 * r[:m])
        hit = None
        for j in range(m - 2):
            if ok[j] and ok[j + 1] and ok[j + 2]:
                hit = j; break
        if hit is None:
            never += 1
        else:
            lat.append(hit * a.fl / sr)
    lat = np.array(lat)
    print("startow=%d bez_SBR_do_konca=%d" % (len(starts), never))
    if len(lat):
        print("opoznienie SBR [s]: min=%.3f sr=%.3f max=%.3f" % (lat.min(), lat.mean(), lat.max()))


if __name__ == "__main__":
    main()
