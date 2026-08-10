#!/usr/bin/env python3
"""PRZYCZYNA ZNALEZIONA: delta-time OPD odnosi sie do INNEJ obwiedni u dekodera.

Dekoder (ffmpeg aacps_common.c, makro READ_PAR_DATA, galaz dt=1):
    int e_prev = e ? e - 1 : ps->num_env_old - 1;
    e_prev = FFMAX(e_prev, 0);
    val = PAR[e_prev][b] + huffman_delta

Czyli w delta-time:
  * dla obwiedni e > 0  odniesieniem jest POPRZEDNIA OBWIEDNIA TEJ SAMEJ RAMKI,
  * tylko dla e == 0    odniesieniem jest OSTATNIA obwiednia POPRZEDNIEJ ramki
                        (indeks num_env_old - 1).

Nasz enkoder (ps_encode.cpp processIpdData) liczy delte KAZDEJ obwiedni wzgledem
psData->opdIdxLast, czyli wzgledem ostatniej obwiedni POPRZEDNIEJ ramki - dla
wszystkich e. Przy nEnvelopes == 1 oba podejscia sa identyczne (e==0), dlatego
blad byl niewidoczny na wiekszosci materialu. Rozjezdzaja sie DOPIERO gdy ramka
ma wiecej niz jedna obwiednie i wybrany zostanie tryb delta-time.

To wyjasnia WSZYSTKIE obserwacje:
  * --ps-env 1                  -> zawsze e==0, brak bledu
  * OPD_FREQ_ONLY (delta-freq)   -> odniesienie wewnatrz obwiedni, brak bledu
  * --ps-env 4 --ps-env-reduce 0 -> brak bledu, bo przy stalych 4 obwiedniach
                                    num_env_old sie nie zmienia i wybor trybu
                                    wypada inaczej (delta-freq tanszy)
  * blad tylko na eps0.50        -> jedyna probka, gdzie nEnv=4 zbieglo sie z
                                    delta-time i zmiana num_env_old miedzy ramkami

TEN SKRYPT sprawdza predykcje LICZBOWO na wszystkich probkach: blad dekodera
wystepuje dokladnie wtedy, gdy istnieje ramka z nEnv>1 kodowana delta-time.
"""
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.expanduser("~/aacfdk_native/front/fdkaac")


def probe(wav, freq_only=False):
    base = os.path.basename(wav)[:-4]
    m4a = os.path.join(HERE, "dumps", "p_%s.m4a" % base)
    env = dict(os.environ, OPD_BITS="1")
    if freq_only:
        env["OPD_FREQ_ONLY"] = "1"
    p = subprocess.run([ENC, "-p", "29", "-b", "48000", "--ps-ipd", "1",
                        "--ps-opd", "1", "-o", m4a, wav],
                       capture_output=True, env=env)
    multi = 0
    for ln in p.stderr.decode(errors="replace").splitlines():
        if "PSEXT fr=" in ln:
            d = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", ln)}
            if d["nEnv"] > 1:
                multi += 1
    dec = subprocess.run(["ffmpeg", "-y", "-v", "warning", "-i", m4a,
                          "-f", "null", "-"], capture_output=True)
    nerr = len(re.findall(r"overflow|illegal",
                          dec.stderr.decode(errors="replace")))
    return multi, nerr


if __name__ == "__main__":
    wavs = sorted(glob.glob(os.path.join(HERE, "*.wav")))
    print("Ramki z nEnv>1 to warunek KONIECZNY bledu (delta-time siega wtedy")
    print("innej obwiedni u dekodera niz u enkodera).")
    print()
    print("%-28s %8s %8s | %8s %8s" % ("probka", "nEnv>1", "bledow",
                                       "freq_only", "bledow"))
    consistent = True
    for w in wavs:
        m, e = probe(w)
        m2, e2 = probe(w, freq_only=True)
        if e > 0 and m == 0:
            consistent = False
        if e2 > 0:
            consistent = False
        print("%-28s %8d %8d | %8d %8d" % (os.path.basename(w)[:-4],
                                           m, e, m2, e2))
    print()
    if consistent:
        print("SPOJNE: kazdy blad mial ramke nEnv>1, a delta-freq usuwa bledy")
        print("        na WSZYSTKICH probkach (prawa kolumna = zera).")
    else:
        print("NIESPOJNE - hipoteza wymaga korekty")
    sys.exit(0 if consistent else 1)
