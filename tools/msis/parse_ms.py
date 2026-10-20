#!/usr/bin/env python3
"""Odczyt maski MS z bitstreamu ADTS (strona dekodera, nie enkodera).

Dla kazdej ramki: pierwszy element musi byc CPE ze wspolnym oknem. Czytamy
ics_info (window_sequence, max_sfb, grupowanie) i ms_mask_present oraz
ms_used[g][sfb]. Tylko stdlib.

Uzycie: parse_ms.py plik.aac [--lo N] [--hi N] [--max N] [--json]
Z --lo/--hi/--max sprawdza zgodnosc maski z zakresem pasm liczonym
W OBREBIE GRUPY (numer SFB czestotliwosciowego), tak jak definiuja to
przelaczniki --msbands-lo/--msbands-hi/--msbands.
"""
import sys, json, argparse


class Bits:
    def __init__(self, data, pos=0):
        self.d = data
        self.p = pos * 8

    def get(self, n):
        v = 0
        for _ in range(n):
            byte = self.d[self.p >> 3]
            v = (v << 1) | ((byte >> (7 - (self.p & 7))) & 1)
            self.p += 1
        return v


def frames(data):
    i = 0
    n = len(data)
    while i + 7 <= n:
        if data[i] != 0xFF or (data[i + 1] & 0xF0) != 0xF0:
            raise ValueError("utracona synchronizacja ADTS na bajcie %d" % i)
        prot_absent = data[i + 1] & 1
        flen = ((data[i + 3] & 3) << 11) | (data[i + 4] << 3) | (data[i + 5] >> 5)
        nblk = data[i + 6] & 3
        hdr = 7 if prot_absent else 9
        yield data[i:i + flen], hdr, nblk
        i += flen


def parse_frame(fr, hdr):
    b = Bits(fr, hdr)
    ele = b.get(3)
    if ele != 1:
        return {"ele": ele}
    b.get(4)  # tag
    common = b.get(1)
    if not common:
        return {"ele": 1, "common": 0}
    b.get(1)  # ics_reserved_bit
    ws = b.get(2)
    b.get(1)  # window_shape
    if ws == 2:  # EIGHT_SHORT_SEQUENCE
        max_sfb = b.get(4)
        grouping = b.get(7)
        groups = 1 + sum(1 for k in range(7) if not (grouping >> (6 - k)) & 1)
    else:
        max_sfb = b.get(6)
        if b.get(1):
            raise ValueError("predictor_data_present w LC")
        groups = 1
    mmp = b.get(2)
    used = None
    if mmp == 1:
        used = [[b.get(1) for _ in range(max_sfb)] for _ in range(groups)]
    return {"ele": 1, "common": 1, "short": ws == 2, "max_sfb": max_sfb,
            "groups": groups, "mmp": mmp, "used": used}


def allowed(sfb, lo, hi, mx):
    if mx is not None and mx >= 0 and sfb >= mx:
        return False
    if lo is not None and lo >= 0 and sfb < lo:
        return False
    if hi is not None and hi >= 0 and sfb > hi:
        return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("f")
    ap.add_argument("--lo", type=int)
    ap.add_argument("--hi", type=int)
    ap.add_argument("--max", type=int)
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    data = open(a.f, "rb").read()
    st = {"frames": 0, "cpe_common": 0, "long": 0, "short": 0,
          "mmp": [0, 0, 0, 0],
          # pasma z MS w grupie 0 / w grupach > 0 (tylko ramki short)
          "short_ms_g0": 0, "short_ms_gN": 0, "short_groups_gt1": 0,
          "long_ms": 0,
          # naruszenia zakresu
          "viol_band": 0,      # ms_used=1 na pasmie poza zakresem
          "viol_all": 0,       # ms_mask_present=2 przy ograniczonym zakresie
          "used_in_range_gN": 0,  # MS w zakresie w grupach >0 (dowod, ze dziala)
          }
    restricted = any(v is not None and v >= 0 for v in (a.lo, a.hi, a.max))
    for fr, hdr, nblk in frames(data):
        st["frames"] += 1
        if nblk != 0:
            continue
        r = parse_frame(fr, hdr)
        if r.get("common") != 1:
            continue
        st["cpe_common"] += 1
        st["short" if r["short"] else "long"] += 1
        st["mmp"][r["mmp"]] += 1
        if r["short"] and r["groups"] > 1:
            st["short_groups_gt1"] += 1
        if r["mmp"] == 2 and restricted:
            # MS na WSZYSTKICH pasmach - narusza, jesli jakies pasmo < max_sfb
            # jest poza dozwolonym zakresem
            if any(not allowed(s, a.lo, a.hi, a.max) for s in range(r["max_sfb"])):
                st["viol_all"] += 1
        if r["mmp"] == 1:
            for g, row in enumerate(r["used"]):
                for s, u in enumerate(row):
                    if not u:
                        continue
                    if r["short"]:
                        st["short_ms_g0" if g == 0 else "short_ms_gN"] += 1
                        if g > 0 and allowed(s, a.lo, a.hi, a.max):
                            st["used_in_range_gN"] += 1
                    else:
                        st["long_ms"] += 1
                    if restricted and not allowed(s, a.lo, a.hi, a.max):
                        st["viol_band"] += 1
    if a.json:
        print(json.dumps(st))
    else:
        for k, v in st.items():
            print("%-18s %s" % (k, v))


if __name__ == "__main__":
    main()
