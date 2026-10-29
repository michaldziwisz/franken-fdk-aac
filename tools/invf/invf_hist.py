#!/usr/bin/env python3
"""Rozklad trybow inverse filtering odczytany PO STRONIE DEKODERA.

Wejscie: plik zrzutu z dekodera Fraunhofera (zmienna INVF_DUMP, drzewo
diagnostyczne), jedna linia na element SBR na ramke, np. "0123" albo
"0123|0011" dla pary kanalow. Wynik: udzial OFF/LOW/MID/HIGH, srednia,
liczba zmian trybu miedzy kolejnymi ramkami (na pasmo), JSON z --json.
"""
import sys, json


def main():
    f = sys.argv[1]
    cnt = [0, 0, 0, 0]
    prev = None
    switches = 0
    total_bands = 0
    frames = 0
    for line in open(f):
        line = line.strip()
        if not line:
            continue
        frames += 1
        modes = [int(c) for c in line.replace("|", "")]
        for m in modes:
            cnt[m] += 1
        total_bands += len(modes)
        if prev is not None and len(prev) == len(modes):
            switches += sum(1 for a, b in zip(prev, modes) if a != b)
        prev = modes
    tot = sum(cnt) or 1
    res = {
        "frames": frames,
        "off": round(cnt[0] / tot, 3), "low": round(cnt[1] / tot, 3),
        "mid": round(cnt[2] / tot, 3), "high": round(cnt[3] / tot, 3),
        "mean": round(sum(i * c for i, c in enumerate(cnt)) / tot, 3),
        "max": max((i for i in range(4) if cnt[i]), default=-1),
        "min": min((i for i in range(4) if cnt[i]), default=-1),
        "switch_per_band_frame": round(switches / max(1, total_bands - (len(prev) if prev else 0)), 3),
        "distinct": sum(1 for c in cnt if c),
    }
    if "--json" in sys.argv:
        print(json.dumps(res))
    else:
        print("ramek %(frames)d  OFF %(off).3f LOW %(low).3f MID %(mid).3f HIGH %(high).3f  "
              "srednia %(mean).2f  min %(min)d max %(max)d  zmian/pasmo/ramke %(switch_per_band_frame).3f" % res)


if __name__ == "__main__":
    main()
