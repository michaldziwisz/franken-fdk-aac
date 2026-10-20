#!/usr/bin/env python3
"""LOAS (AudioSyncStream): w ktorych ramkach jest StreamMuxConfig
(useSameStreamMux == 0), czyli konfiguracja ASC - dla AAC-ELD to ona niesie
ld_sbr_header, wiec to jej okres decyduje o wejsciu SBR w srodek strumienia.
Tylko stdlib.

Uzycie: loas_smc.py plik.loas [--sr 48000] [--fl 512] [--json]
"""
import argparse, json


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("f")
    ap.add_argument("--sr", type=int, default=48000)
    ap.add_argument("--fl", type=int, default=512)
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    d = open(a.f, "rb").read()
    i = 0; k = 0; smc = []
    while i + 3 <= len(d):
        sync = (d[i] << 3) | (d[i + 1] >> 5)
        if sync != 0x2B7:
            raise ValueError("utracony sync LOAS na %d" % i)
        ln = ((d[i + 1] & 0x1F) << 8) | d[i + 2]
        if ln and not (d[i + 3] >> 7):  # useSameStreamMux == 0
            smc.append(k)
        i += 3 + ln; k += 1
    gaps = [b - x for x, b in zip(smc, smc[1:])]
    fs = a.fl / a.sr
    r = {"frames": k, "smc": len(smc), "first": smc[:6],
         "gap_min": min(gaps) if gaps else None, "gap_max": max(gaps) if gaps else None,
         "gap_max_s": round(max(gaps) * fs, 3) if gaps else None}
    print(json.dumps(r) if a.json else "\n".join("%-10s %s" % kv for kv in r.items()))


if __name__ == "__main__":
    main()
