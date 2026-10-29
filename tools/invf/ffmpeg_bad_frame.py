#!/usr/bin/env python3
"""Ktora ramka ADTS wywoluje blad ffmpeg? Bisekcja po prefiksach strumienia.

Uzycie: ffmpeg_bad_frame.py plik.aac
Wypisuje liczbe ramek i numer pierwszej ramki, po ktorej ffmpeg zglasza blad
(error|invalid|overflow|illegal), oraz tresc komunikatu.
"""
import re, subprocess, sys, tempfile, os


def frames(data):
    out = []; i = 0
    while i + 7 <= len(data):
        if data[i] != 0xFF or (data[i + 1] & 0xF0) != 0xF0:
            i += 1; continue
        ln = ((data[i + 3] & 3) << 11) | (data[i + 4] << 3) | (data[i + 5] >> 5)
        if ln < 7: break
        out.append((i, ln)); i += ln
    return out


def errs(blob):
    with tempfile.NamedTemporaryFile(suffix=".aac", delete=False, dir=os.environ.get("TMPDIR")) as f:
        f.write(blob); p = f.name
    e = subprocess.run(["ffmpeg", "-v", "error", "-i", p, "-f", "null", "-"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE).stderr.decode("latin1")
    os.remove(p)
    return [l for l in e.splitlines() if re.search(r"error|invalid|overflow|illegal", l, re.I)]


def main():
    data = open(sys.argv[1], "rb").read()
    fr = frames(data)
    pref = lambda n: data[:fr[n - 1][0] + fr[n - 1][1]]
    if not errs(data):
        print("ramek %d, ffmpeg bez bledow" % len(fr)); return
    lo, hi = 0, len(fr)
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if errs(pref(mid)): hi = mid
        else: lo = mid
    print("ramek %d, pierwszy blad po ramce %d (0-based %d): %s" % (len(fr), hi, hi - 1, errs(pref(hi))[0]))


if __name__ == "__main__":
    main()
