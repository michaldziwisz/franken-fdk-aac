#!/bin/bash
# Dokladny odcisk 2 ramek, ktore wywoluja blad + kontrola DRUGIM dekoderem.
# Pytanie: czy to nasz bug w bitstreamie, czy tylko surowosc ffmpega?
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
S=A_antiphase_eps0.50

OPD_BITS=1 "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 \
    -o dumps/bug.m4a "$S.wav" 2> dumps/bug.bits

echo "=== ramki z nEnv=4 (podejrzane) ==="
grep 'nEnv=4' dumps/bug.bits

echo
echo "=== ramki z naglowkiem PS (hdr=1) - pierwsze 6 ==="
grep 'hdr=1' dumps/bug.bits | head -6

echo
echo "=== ile ramek zmienia iidMode wzgledem poprzedniej ==="
awk '/PSEXT/{for(i=1;i<=NF;i++){split($i,a,"=");v[a[1]]=a[2]}
  if(NR>1 && v["iidMode"]!=prev) c++; prev=v["iidMode"]}END{print c+0}' dumps/bug.bits

echo
echo "=== ffmpeg: dokladne komunikaty ==="
ffmpeg -y -v warning -i dumps/bug.m4a -f null - 2>&1 | head -6

echo
echo "=== KONTROLA drugim dekoderem (faad) ==="
if command -v faad >/dev/null 2>&1; then
  faad -o dumps/bug_faad.wav dumps/bug.m4a 2>&1 | tail -4
  ls -la dumps/bug_faad.wav 2>/dev/null || echo "faad nie wyprodukowal pliku"
else
  echo "faad NIEDOSTEPNY - brak niezaleznej kontroli"
fi

echo
echo "=== czy stock fdkaac2.exe / OPD=0 tez daje blad (baseline) ==="
"$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 0 -o dumps/base.m4a "$S.wav" 2>/dev/null
ffmpeg -y -v warning -i dumps/base.m4a -f null - 2>&1 | head -3
echo "(pusto = brak bledow)"
