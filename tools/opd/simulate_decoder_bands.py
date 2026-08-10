#!/usr/bin/env python3
"""SYMULACJA STANU DEKODERA - domkniecie watku --ps-env 4.

HIPOTEZA (ze zrodla ffmpeg aacps_common.c:143-156):
    if (header) {
        enable_iid = get_bits1();
        if (enable_iid) { iid_mode = ...; nr_ipdopd_par = nr_iidopd_par_tab[iid_mode]; }
        ...
    }
Czyli dekoder aktualizuje LICZBE PASM IPD/OPD wylacznie wtedy, gdy przyszedl
NAGLOWEK i to z enable_iid == 1. Nasz writer liczy ja ZAWSZE z biezacego
psOut->iidMode (getNoIpdOpdBands). Jesli wiec:
  * przyjdzie naglowek z enable_iid = 0, albo
  * iidMode zmieni sie w ramce BEZ naglowka,
to enkoder zapisze N pasm fazy, a dekoder odczyta M != N -> licznik rozszerzenia
schodzi ponizej zera ("ps extension overflow -2/-4") i psuje parse ICC.

Ten skrypt odtwarza automat dekodera na podstawie dumpu enkodera i liczy ramki
rozjazdu. PREDYKCJA FALSYFIKOWALNA: liczba ramek rozjazdu > 0 dokladnie wtedy,
gdy ffmpeg zglasza bledy.
"""
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.expanduser("~/aacfdk_native/front/fdkaac")
# nr_iidopd_par_tab[] z ffmpeg (indeksowane iid_mode 0..5)
NR_IPDOPD = [5, 11, 17, 5, 11, 17]


def run(wav, extra):
    env = dict(os.environ, OPD_BITS="1")
    m4a = os.path.join(HERE, "dumps", "sim.m4a")
    p = subprocess.run([ENC, "-p", "29", "-b", "48000", "--ps-ipd", "1",
                        "--ps-opd", "1"] + extra + ["-o", m4a, wav],
                       capture_output=True, env=env)
    rows = []
    for ln in p.stderr.decode(errors="replace").splitlines():
        if "PSEXT fr=" in ln:
            rows.append({k: int(v) for k, v in
                         re.findall(r"(\w+)=(-?\d+)", ln)})
    d = subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", m4a,
                        "-f", "null", "-"], capture_output=True)
    log = d.stderr.decode(errors="replace")
    nerr = len(re.findall(r"overflow|illegal", log))
    return rows, nerr


def simulate(rows):
    """Odtworz nr_ipdopd_par dekodera i policz ramki rozjazdu."""
    dec_bands = 0          # stan dekodera na starcie strumienia
    mismatch = 0
    detail = []
    for r in rows:
        # dekoder: aktualizuje TYLKO gdy naglowek ORAZ enable_iid
        if r["hdr"] == 1 and r["iid"] == 1:
            dec_bands = NR_IPDOPD[r["iidMode"]] if r["iidMode"] < 6 else 5
        enc_bands = r["bands"]          # co ZAPISAL enkoder
        if enc_bands != dec_bands:
            mismatch += 1
            if len(detail) < 3:
                detail.append((r["fr"], enc_bands, dec_bands, r["nEnv"],
                               r["hdr"], r["iid"]))
    return mismatch, detail


if __name__ == "__main__":
    variants = [([], "domyslne"),
                (["--ps-env", "4", "--ps-env-reduce", "0"], "env4")]
    wavs = sorted(glob.glob(os.path.join(HERE, "*.wav"))) + \
        sorted(glob.glob(os.path.join(HERE, "real", "M*.wav")))
    print("Symulacja stanu dekodera: ile ramek zapisuje INNA liczbe pasm fazy,")
    print("niz dekoder w tym momencie oczekuje.")
    print()
    print("%-26s %-10s %8s %10s  %s" % ("probka", "wariant", "rozjazd",
                                        "bledy_ff", "zgodnosc"))
    consistent = True
    for w in wavs:
        for extra, lbl in variants:
            rows, nerr = run(w, extra)
            mm, det = simulate(rows)
            agree = (mm > 0) == (nerr > 0)
            consistent = consistent and agree
            print("%-26s %-10s %8d %10d  %s"
                  % (os.path.basename(w)[:-4], lbl, mm, nerr,
                     "OK" if agree else "!!! SPRZECZNOSC"))
            for fr, e, d, ne, h, i in det:
                print("      fr=%d enkoder=%d pasm, dekoder=%d, nEnv=%d hdr=%d iid=%d"
                      % (fr, e, d, ne, h, i))
    print()
    print("PREDYKCJA %s" % ("POTWIERDZONA" if consistent else "OBALONA"))
    sys.exit(0 if consistent else 1)
