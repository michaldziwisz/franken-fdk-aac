#!/usr/bin/env python3
"""Znajdz numer pierwszej ramki ADTS, po ktorej dekoder zglasza blad.

Dekoduje rosnace prefiksy strumienia (wyszukiwanie binarne) przez ffmpeg.
Uzycie: find_bad_frame.py plik.aac
"""
import subprocess, sys, os

PAT = ("error", "invalid", "exceeds", "overflow", "illegal")


def frames(data):
    out = []; i = 0
    while i + 7 <= len(data):
        flen = ((data[i + 3] & 3) << 11) | (data[i + 4] << 3) | (data[i + 5] >> 5)
        out.append((i, flen)); i += flen
    return out


def errs(buf, tmp):
    open(tmp, "wb").write(buf)
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", tmp, "-f", "null", "-"],
                       capture_output=True, text=True)
    return [l for l in r.stderr.splitlines() if any(p in l.lower() for p in PAT)]


def main():
    data = open(sys.argv[1], "rb").read()
    fr = frames(data)
    tmp = sys.argv[1] + ".prefix.aac"
    if not errs(data, tmp):
        print("brak bledow"); return
    lo, hi = 1, len(fr)
    while lo < hi:
        mid = (lo + hi) // 2
        end = fr[mid - 1][0] + fr[mid - 1][1]
        if errs(data[:end], tmp):
            hi = mid
        else:
            lo = mid + 1
    os.remove(tmp)
    print("pierwszy blad po ramce", lo, "z", len(fr))


if __name__ == "__main__":
    main()
