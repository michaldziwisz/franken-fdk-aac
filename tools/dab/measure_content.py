#!/usr/bin/env python3
"""Dowod TRESCI: porownanie oryginalu z dekodowanym WAV, per-kanal, z
wyrownaniem opoznienia kodera/dekodera (priming) przez korelacje.

Uzycie: measure_content.py <oryginal.wav> <zdekodowany.wav>

PULAPKI (z pamieci projektu):
- Naiwny RMS bez wyrownania lag mierzy FAZE, nie blad kwantyzacji -> bezwartosciowy.
  Dlatego szukamy lag korelacja NA STABILNYM kanale, potem mierzymy oba tym samym lagiem.
- Do dowodu uzywaj TONOW (sinus) w obu kanalach - dla nich per-sample RMS ma sens.
  PINK NOISE ma wysoki 'blad' mimo poprawnego kodowania (AAC koduje szum
  percepcyjnie: inne probki, podobna energia) - do szumu porownuj ENERGIE/widmo,
  nie per-sample.
Prog akceptacji dla tonow: err/orig < ~2% = dekoder poprawnie odtworzyl strumien.
"""
import sys, numpy as np, wave

def load(p):
    w = wave.open(p, 'rb')
    d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64)
    d = d.reshape(-1, w.getnchannels()); w.close()
    return d

def main():
    o = load(sys.argv[1]); l = load(sys.argv[2])
    print(f"  orig {o.shape}  dekod {l.shape}")
    # wspolny lag z kanalu 0 (zakladamy stabilny ton tam) - wektoryzowana korelacja
    maxlag = min(10000, len(l) - 40000)
    seg = o[20000:40000, 0]
    corr = np.correlate(l[20000:40000+maxlag, 0], seg, mode='valid')
    bl = int(np.argmax(corr))
    print(f"  wspolny lag (probki): {bl}")
    ok = True
    for ch in range(min(o.shape[1], l.shape[1])):
        a = o[20000:60000, ch]; b = l[20000+bl:60000+bl, ch]
        m = min(len(a), len(b)); a, b = a[:m], b[:m]
        ro = np.sqrt(np.mean(a**2)); er = np.sqrt(np.mean((a-b)**2))
        pct = 100*er/(ro+1e-9)
        flag = "OK" if pct < 2.0 else "SPRAWDZ (szum? -> mierz energie)"
        print(f"  kanal {ch}: RMS_orig={ro:8.0f}  err={pct:6.1f}%  [{flag}]")
        if pct >= 2.0: ok = False
    print("CONTENT_PROOF_PASS" if ok else "CONTENT_PROOF_CHECK")

if __name__ == "__main__":
    main()
