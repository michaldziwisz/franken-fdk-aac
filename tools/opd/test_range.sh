#!/bin/bash
# Czy delta wypada poza tablice Huffmana (RANGEERR) i czy koreluje z bledem?
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
for b in A_antiphase_eps0.50 D1_wide_bass_in_phase A_antiphase_eps0.00 D2_reverb_decorrelated; do
  OPD_RANGE=1 "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 \
      -o "dumps/rng_$b.m4a" "$b.wav" 2> "dumps/rng_$b.err"
  n=$(grep -c RANGEERR "dumps/rng_$b.err" || true)
  e=$(ffmpeg -y -v warning -i "dumps/rng_$b.m4a" -f null - 2>&1 | grep -cE 'overflow|illegal' || true)
  printf '%-26s RANGEERR=%-5s bledow_dekodera=%s\n' "$b" "$n" "$e"
  grep RANGEERR "dumps/rng_$b.err" | head -3 | sed 's/^/    /'
done
