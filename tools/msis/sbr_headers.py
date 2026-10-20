#!/usr/bin/env python3
"""Ktore ramki ADTS niosa sbr_header() (bs_header_flag=1). Tylko stdlib.

FDK zapisuje rozszerzenie SBR jako element FIL, a za nim moga stac kolejne
elementy FIL (wypelnienie CBR). Szukamy elementu FIL z typem SBR, za ktorym
lancuch FIL-ow konczy sie ID_END i wyrownaniem do bajtu (<8 zerowych bitow).

Uzycie: sbr_headers.py plik.aac [--sr 44100] [--fl 2048] [--json]
  sr/fl = czestotliwosc i liczba probek WYJSCIOWYCH na ramke (do przeliczen).
"""
import sys, argparse, json


def bits_of(buf):
    out = []
    for b in buf:
        for k in range(7, -1, -1):
            out.append((b >> k) & 1)
    return out


def val(bs, p, n):
    v = 0
    for i in range(n):
        v = (v << 1) | bs[p + i]
    return v


def frames(data):
    i = 0
    while i + 7 <= len(data):
        if data[i] != 0xFF or (data[i + 1] & 0xF0) != 0xF0:
            raise ValueError("sync ADTS utracony na %d" % i)
        prot_absent = data[i + 1] & 1
        flen = ((data[i + 3] & 3) << 11) | (data[i + 4] << 3) | (data[i + 5] >> 5)
        hdr = 7 if prot_absent else 9
        yield data[i + hdr:i + flen]
        i += flen


def fil_at(bs, p):
    """Element FIL zaczynajacy sie w p -> (poczatek payloadu, koniec) albo None."""
    n = len(bs)
    if p + 7 > n or val(bs, p, 3) != 6:
        return None
    q = p + 3
    cnt = val(bs, q, 4); q += 4
    if cnt == 15:
        if q + 8 > n:
            return None
        cnt += val(bs, q, 8) - 1; q += 8
    end = q + cnt * 8
    if end > n:
        return None
    return q, end


def closes(bs, p):
    """Czy od p jest lancuch FIL* ID_END i tylko zera do konca bajtu."""
    n = len(bs)
    for _ in range(64):
        if p + 3 > n:
            return False
        t = val(bs, p, 3)
        if t == 7:
            rest = bs[p + 3:]
            return len(rest) < 8 and not any(rest)
        if t != 6:
            return False
        f = fil_at(bs, p)
        if f is None:
            return False
        p = f[1]
    return False


def sbr_flag(payload):
    bs = bits_of(payload)
    best = None
    for p in range(len(bs) - 12):
        f = fil_at(bs, p)
        if f is None:
            continue
        q, end = f
        if end - q < 8:
            continue
        et = val(bs, q, 4)
        if et == 0b1101:
            flag = bs[q + 4]
        elif et == 0b1110 and end - q >= 15:
            flag = bs[q + 4 + 10]
        else:
            continue
        if closes(bs, end):
            best = flag
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("f")
    ap.add_argument("--sr", type=int, default=44100)
    ap.add_argument("--fl", type=int, default=2048)
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    data = open(a.f, "rb").read()
    hdr = []; nofil = 0; total = 0
    for k, pl in enumerate(frames(data)):
        total += 1
        f = sbr_flag(pl)
        if f is None:
            nofil += 1
        elif f == 1:
            hdr.append(k)
    gaps = [b - x for x, b in zip(hdr, hdr[1:])]
    fs = a.fl / a.sr
    r = {"frames": total, "no_sbr_fil": nofil, "headers": len(hdr),
         "first": hdr[:6], "gap_min": min(gaps) if gaps else None,
         "gap_max": max(gaps) if gaps else None,
         "gap_max_s": round(max(gaps) * fs, 3) if gaps else None}
    if a.json:
        print(json.dumps(r))
    else:
        for k, v in r.items():
            print("%-10s %s" % (k, v))


if __name__ == "__main__":
    main()
