#!/usr/bin/env python3
"""Pasma danego typu (np. IS = ksiazki 14,15; PNS = 13) z logu FRANKEN_BANDLOG.

Log pisze enkoder, gdy ustawiona jest zmienna srodowiskowa FRANKEN_BANDLOG=<plik>
(przez WSL do .exe: WSLENV=FRANKEN_BANDLOG). Linia: ramka kanal L|S grupa pasmo
liczba ksiazka; pochodzi z section_data zapisywanego do strumienia.

Uzycie: bandlog.py log 14,15 [--ch 1] -> JSON:
  long / short: liczba wystapien na numerze pasma (w obrebie grupy)
  short_groups: liczba grup okien w ramkach short
  short_groups_with: grupy z co najmniej jednym pasmem tego typu
  later_groups_with: to samo dla grup > 0
  max_per_group / long_max_per_frame: najwieksza liczba pasm w jednej grupie / ramce
  short_min_band / short_max_band: zakres numerow pasm short
"""
import sys, collections, json, argparse


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log"); ap.add_argument("books"); ap.add_argument("--ch", type=int, default=1)
    a = ap.parse_args()
    books = {int(x) for x in a.books.split(",")}
    frames = collections.defaultdict(list)
    for ln in open(a.log):
        x = ln.split()
        if len(x) == 7:
            frames[int(x[0])].append((int(x[1]), x[2], int(x[3]), int(x[4]), int(x[5]), int(x[6])))
    L = collections.Counter(); S = collections.Counter()
    sg = sgw = lgw = mpg = lmax = 0
    for k, secs in frames.items():
        per = collections.defaultdict(set); typ = None; groups = set()
        for c, t, g, b, n, cb in secs:
            if c != a.ch:
                continue
            typ = t; groups.add(g)
            if cb in books:
                per[g].update(range(b, b + n))
        if typ is None:
            continue
        if typ == "S":
            sg += len(groups)
            for g in groups:
                if per.get(g):
                    sgw += 1
                    if g > 0:
                        lgw += 1
                    mpg = max(mpg, len(per[g]))
                for x in per.get(g, ()):
                    S[x] += 1
        else:
            for x in per.get(0, ()):
                L[x] += 1
            lmax = max(lmax, len(per.get(0, ())))
    print(json.dumps({"long": dict(sorted(L.items())), "short": dict(sorted(S.items())),
                      "short_groups": sg, "short_groups_with": sgw, "later_groups_with": lgw,
                      "max_per_group": mpg, "long_max_per_frame": lmax,
                      "short_min_band": min(S) if S else -1, "short_max_band": max(S) if S else -1}))


if __name__ == "__main__":
    main()
