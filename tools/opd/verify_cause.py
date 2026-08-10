#!/usr/bin/env python3
"""WERYFIKACJA PRZYCZYNY: IPD/OPD wysylane gdy enable_iid=0.

HIPOTEZA (z lektury zrodel dekodera, ffmpeg aacps_common.c:143-155):
Dekoder ustawia nr_ipdopd_par (liczbe pasm IPD/OPD) WYLACZNIE wewnatrz
  if (header) { if (ps->enable_iid) { ... nr_ipdopd_par = nr_iidopd_par_tab[iid_mode]; } }
czyli liczba pasm fazy jest pochodna IID_MODE. Jesli enkoder wysyla rozszerzenie
IPD/OPD w ramce, w ktorej enable_iid=0, dekoder ma nr_ipdopd_par = 0 (albo
resztke) i czyta INNA liczbe bitow niz zapisal enkoder -> licznik rozszerzenia
schodzi ponizej zera ("ps extension overflow") i psuje sie parsowanie ICC.

U nas enableIID = hPsData->iidEnable, ktore ps_encode ustawia z heurystyki
loudnDiff - na poczatku pliku (zanim panorama sie rozjedzie) bywa 0, a my mimo
to wysylamy faze.

PREDYKCJA FALSYFIKOWALNA:
  liczba ramek (nEnv>0 AND iid=0) > 0  <=>  dekoder zglasza blad
Jesli w probce bez bledu ZNAJDA sie takie ramki, hipoteza jest FALSZYWA.
"""
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.expanduser("~/aacfdk_native/front/fdkaac")


def encode_and_probe(wav, extra):
    base = os.path.basename(wav)[:-4]
    m4a = os.path.join(HERE, "dumps", "v_%s.m4a" % base)
    env = dict(os.environ, OPD_BITS="1")
    p = subprocess.run([ENC, "-p", "29", "-b", "48000", "--ps-ipd", "1"] +
                       extra + ["-o", m4a, wav],
                       capture_output=True, env=env)
    rows = []
    for ln in p.stderr.decode(errors="replace").splitlines():
        if "PSEXT fr=" in ln:
            d = dict(re.findall(r"(\w+)=(-?\d+)", ln))
            rows.append({k: int(v) for k, v in d.items()})
    dec = subprocess.run(["ffmpeg", "-y", "-v", "warning", "-i", m4a,
                          "-f", "null", "-"], capture_output=True)
    log = dec.stderr.decode(errors="replace")
    nerr = len(re.findall(r"overflow|illegal", log))
    bad = sum(1 for r in rows if r["nEnv"] > 0 and r["iid"] == 0)
    return bad, nerr, len(rows)


if __name__ == "__main__":
    wavs = sorted(glob.glob(os.path.join(HERE, "*.wav")))
    print("PREDYKCJA: ramki(nEnv>0 & iid=0) > 0  <=>  bledy dekodera > 0")
    print()
    print("%-30s %10s %8s %8s  %s" % ("probka", "zlych_ram", "bledow",
                                      "ramek", "zgodnosc"))
    ok = True
    for w in wavs:
        bad, nerr, tot = encode_and_probe(w, ["--ps-opd", "1"])
        agree = (bad > 0) == (nerr > 0)
        ok = ok and agree
        print("%-30s %10d %8d %8d  %s"
              % (os.path.basename(w)[:-4], bad, nerr, tot,
                 "OK" if agree else "!!! SPRZECZNOSC - hipoteza falszywa"))
    print()
    print("PREDYKCJA %s" % ("POTWIERDZONA na wszystkich probkach" if ok
                            else "OBALONA"))
    sys.exit(0 if ok else 1)
